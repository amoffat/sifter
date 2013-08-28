/*
 * Copyright (C) 2013 Andrew Moffat
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <functional>
#include <unistd.h>
#include <tuple>
#include <numeric>
#include <functional>
#include <csignal>
#include <limits>
#include <cstdio>
#include <cctype>

#include <opencv2/opencv.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/nonfree/features2d.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <tbb/tbb.h>

#include <glib-2.0/glib.h>

#include "sifter.h"
#include "logging.h"
#include "web_server.h"



using namespace cv;


path DATA_DIR;

Server server;
std::map<int, DesignInfo> MatchInfo::designInfoData;
path MatchInfo::designThumbsDir;


void computeKeypointsAndDescriptors(const path &imageFile,
        std::vector<KeyPoint> &keypoints, Mat &descriptors, SIFT &sifter) {

    Mat img = imread(imageFile.string(), CV_LOAD_IMAGE_GRAYSCALE);
    sifter.detect(img, keypoints);
    sifter.compute(img, keypoints, descriptors);
}

Mat computeDescriptors(const path &imageFile, SIFT &sifter) {
    Mat img = imread(imageFile.string(), CV_LOAD_IMAGE_GRAYSCALE);

    std::vector<KeyPoint> keypoints;
    Mat descriptors;

    sifter.detect(img, keypoints);
    sifter.compute(img, keypoints, descriptors);

    return descriptors;
}


void saveDescriptorsAndKeypoints(const path &fileName, const Mat &descriptors,
        const std::vector<KeyPoint> &keypoints) {
    FileStorage handle;
    handle.open(fileName.string(), FileStorage::WRITE);
    handle << "descriptors" << descriptors;
    handle << "keypoints" << keypoints;
    handle.release();
}


Mat loadDescriptors(const path &fileName) {
    FileStorage handle;
    Mat descriptors;

    handle.open(fileName.string(), FileStorage::READ);
    handle["descriptors"] >> descriptors;
    handle.release();

    return descriptors;
}

/*
 * takes our yaml design info file and populates a mapping of structs containing
 * that data
 */
std::map<int, DesignInfo> loadDesignInfoData(const path &fileName) {
    FileStorage handle;

    handle.open(fileName.string(), FileStorage::READ);
    std::string artist;

    std::map<int, DesignInfo> productData;
    FileNode mapping = handle["product_mapping"];
    for (auto entry: mapping) {
        int i = std::stoi(entry.name().substr(1, std::string::npos));
        DesignInfo designInfo;

        designInfo.id = i;
        entry["artist"] >> designInfo.artistName;
        entry["title"] >> designInfo.title;
        entry["added"] >> designInfo.dateAdded;
        entry["url"] >> designInfo.artistUrl;

        productData[i] = designInfo;
    }
    handle.release();
    return productData;
}


void applyFunctionToImages(const path &imageDirectory,
        std::function<void(const path&)> application, int max) {

    auto it = recDirIt(imageDirectory);
    auto end = recDirIt();

    if (max == 0) {
        max = std::numeric_limits<int>::max();
    }

    int i = 1;
    std::string validExt = ".jpg";

    while (it != end && i <= max) {
        path imagePath = (*it).path();
        auto ext = imagePath.extension().string();
        it++;

        if (ext != validExt) {
            continue;
        }
        dlog("processing " << i++ << " " << imagePath, logging::HIGH);
        application(imagePath);
    }
}


void generateKeypointsAndDescriptors(const path& imagePath,
        const path& descriptorDir, SIFT &sifter, bool skipExists) {

    path descriptorFile = descriptorDir / (imagePath.filename().string() + ".sift");

    if (boost::filesystem::exists(descriptorFile) && skipExists) {
        dlog(descriptorFile << " already exists, skipping", logging::HIGH);
        return;
    }

    Mat descriptors;
    std::vector<KeyPoint> keypoints;

    computeKeypointsAndDescriptors(imagePath, keypoints, descriptors, sifter);
    saveDescriptorsAndKeypoints(descriptorFile, descriptors, keypoints);
}



MatchDetails compareImageToDesign(const Mat &query, const Mat &training,
        DescriptorMatcher &matcher, float distanceRatioThreshold) {
    std::vector<DMatch> matches;

    std::vector<std::vector<DMatch>> knnMatches;

    dlog("beginning matching", logging::LOW);
    matcher.knnMatch(query, training, knnMatches, 2);


    /*
     * filter out matches based on our distance ratio threshold of the nearest
     * match and the next nearest match
     */
    std::vector<DMatch> goodMatches;
    for (auto matchGroup: knnMatches) {
        float ratio = matchGroup[0].distance / matchGroup[1].distance;
        if (ratio > distanceRatioThreshold) {
            continue;
        }
        goodMatches.push_back(matchGroup[0]);
    }
    dlog("done matching", logging::LOW);


    /*
     * now filter out all matches that have multiple matches at this location.
     * multiple matchces at a single location is a good indicator of a bad
     * match, so removing all of the dups should lower the total number of
     * matches enough that this does not get selected as a good match.  we
     * could use a set, but that will not filter out all duplicated matches,
     * since it will leave 1 match at the location where many were
     */
    std::map<int, int> identicalMatches;
    for (auto match: goodMatches) {
        identicalMatches[match.trainIdx] += 1;
    }
    for (auto match: goodMatches) {
        if (identicalMatches[match.trainIdx] <= 1) {
            matches.push_back(match);
        }
    }


    MatchDetails details;
    details.numMatches = matches.size();
    for (auto match: matches) {
        details.totalDistance += match.distance;
    }
    details.averageDistance = details.totalDistance / details.numMatches;
    return details;
}


/*
 * used for creating a unique set of DMatches, based on uniqueness in the
 * training image match location
 */
struct compareDMatch {
    bool operator()(const DMatch &m1, const DMatch &m2) {
        return m1.trainIdx < m2.trainIdx;
    }
};


std::ostream &operator<<(std::ostream &out, const DMatch &match) {
    out << "<DMatch"
        << " " << match.queryIdx << " -> " << match.trainIdx
        << ": " << match.distance
        << ">";
    return out;
}


/*
 * used for testing how well different parameters and filtering methods work.
 * it doesn't really use much matching code found elsewhere, because it is
 * designed to be a little sandbox area to try out different methods and
 * optimizations
 */
void debugMatch(const path &imgFile1, const path &imgFile2,
        float distanceRatioThreshold, SIFT &sifter) {

    BFMatcher matcher(NORM_L2, false);

    Mat img1 = imread(imgFile1.string(), CV_LOAD_IMAGE_GRAYSCALE);
    Mat img2 = imread(imgFile2.string(), CV_LOAD_IMAGE_GRAYSCALE);
    std::vector<KeyPoint> keypoints1, keypoints2;

    sifter.detect(img1, keypoints1);
    sifter.detect(img2, keypoints2);

    Mat descriptors1;
    Mat descriptors2;
    sifter.compute(img1, keypoints1, descriptors1);
    sifter.compute(img2, keypoints2, descriptors2);


    std::vector<std::vector<DMatch>> knnMatches;
    dlog("beginning matching", logging::HIGH);

    matcher.knnMatch(descriptors1, descriptors2, knnMatches, 2);

    /*
     * filter out based on ratio of first match to second match
     */
    std::vector<DMatch> goodMatches;
    for (auto matchGroup: knnMatches) {
        float ratio = matchGroup[0].distance / matchGroup[1].distance;
        if (ratio > distanceRatioThreshold) {
            continue;
        }
        goodMatches.push_back(matchGroup[0]);
    }
    dlog("done matching", logging::HIGH);


    /*
     * now filter out all matches that have multiple matches at this location
     */
    std::map<int, int> identicalMatches;
    std::vector<DMatch> uniqueMatchesVector;
    for (auto match: goodMatches) {
        identicalMatches[match.trainIdx] += 1;
    }
    for (auto match: goodMatches) {
        if (identicalMatches[match.trainIdx] <= 1) {
            uniqueMatchesVector.push_back(match);
        }
    }


    std::vector<DMatch> *matches = &uniqueMatchesVector;

    dlog(matches->size() << " matches", logging::HIGH);

    Mat imgMatches;
    drawMatches(img1, keypoints1, img2, keypoints2,
        *matches, imgMatches, Scalar::all(-1), Scalar::all(-1),
        vector<char>(), DrawMatchesFlags::DRAW_RICH_KEYPOINTS |
                        DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );

    imshow("matches", imgMatches);
    waitKey(0);

    imwrite("/tmp/sifter.png", imgMatches);
}


PotentialMatch::PotentialMatch() {
    details.numMatches = 0;
    details.averageDistance = std::numeric_limits<float>::max();
}

bool PotentialMatch::operator<(const PotentialMatch &m2) {
    return details.numMatches < m2.details.numMatches;
}

bool PotentialMatch::operator<(int minMatches) {
    return details.numMatches < minMatches;
}

bool PotentialMatch::operator>=(const PotentialMatch &m2) {
    return details.numMatches >= m2.details.numMatches;
}

bool PotentialMatch::operator>(const PotentialMatch &m2) {
    return details.numMatches > m2.details.numMatches;
}

bool operator>(const PotentialMatch &m1, const PotentialMatch &m2) {
    return m1.details.numMatches > m2.details.numMatches;
}



std::ostream &operator<<(std::ostream &out, const PotentialMatch &match) {
    out << "<PotentialMatch"
        << " " << match.id << ": "
        << match.details.numMatches
        << ", confidence: " << match.confidence
        << ">";
    return out;
}



MatchInfo::MatchInfo(const PotentialMatch &match, float elapsed): match(match),
        elapsed(elapsed) {
    design = designInfoData[match.id];

    designUrl = "http://www.threadless.com/product/" + std::to_string(match.id);

    path designFile = designThumbsDir/(std::to_string(match.id) + ".jpg");
    std::ifstream design(designFile.string(), std::ifstream::binary);
    Mat designMat = cv::imread(designFile.string());
    width = designMat.cols;
    height = designMat.rows;

    design.seekg(0, std::ios::end);
    int length = design.tellg();
    design.seekg(0, std::ios::beg);

    char *buffer = new char[length];
    design.read(buffer, length);
    design.close();

    thumbnail = g_base64_encode((const uchar*)buffer, length);
    delete buffer;
}


/*
 * serializes ourself to JSON.  we use double quotes, which should be fine,
 * because our product data has been pre-sanitized for double quotes
 */
std::string MatchInfo::json() {
    std::stringstream jsonBuf;
    jsonBuf << "{"
        << "\"id\": " << design.id
        << ", \"design_url\": \"" << designUrl << "\""
        << ", \"title\": \"" << design.title << "\""
        << ", \"artist\": \"" << design.artistName << "\""
        << ", \"added\": \"" << design.dateAdded << "\""
        << ", \"artist_url\": \"" << design.artistUrl << "\""
        << ", \"confidence\": " << match.confidence
        << ", \"elapsed\": " << elapsed
        << ", \"thumbnail\": \"" << thumbnail << "\""
        << ", \"width\": " << width
        << ", \"height\": " << height
        << "}";
    return jsonBuf.str();
}



std::vector<Mat> preloadDescriptors(const path &descriptorDirectory) {
    std::vector<Mat> preloaded;

    dlog("preloading descriptors from " << descriptorDirectory, logging::HIGH);


    auto it = dirIt(descriptorDirectory);
    auto end = dirIt();
    int maxId = 0;
    while (it != end) {
        path descriptorPath = (*it).path();
        it++;

        int id = std::stoi(descriptorPath.stem().string());
        if (id > maxId) {
            maxId = id;
        }
    }

    for (int i=0; i<maxId; i++){
        path descriptorPath = descriptorDirectory/(std::to_string(i) + ".jpg.sift");
        dlog("loading " << descriptorPath, logging::LOW);

        Mat descriptors = loadDescriptors(descriptorPath);
        preloaded.push_back(descriptors);
    }

    dlog("done preloading", logging::HIGH);

    return preloaded;
}


/*
 * takes an input file path and returns the best match it can find for that
 * design
 */
MatchInfo findBestMatch(const path &fileNameToMatch,
        const std::vector<Mat> &descriptors, int numBestMatches,
        float distanceRatioThreshold, SIFT &sifter, SIFT &refineSifter,
        bool multithreaded) {

    dlog("finding single best match for " << fileNameToMatch, logging::HIGH);

    double start = logging::timestamp();
    auto matches = findBestMatches(fileNameToMatch, descriptors, numBestMatches,
        distanceRatioThreshold, sifter, multithreaded);
    PotentialMatch bestMatch = ofBestMatchesGetOne(fileNameToMatch, descriptors,
        matches, refineSifter);
    double elapsed = logging::timestamp() - start;

    dlog("best match " << bestMatch << " for " << fileNameToMatch
        << " took " << elapsed << " seconds", logging::HIGH);

    MatchInfo info(bestMatch, elapsed);

    return info;
}


/*
 * whittles down a list of PotentialMatches to a single PotentialMatch by
 * performing additional checks and optimizations.  the resulting match has
 * its confidence value set according to the results of some training data
 */
PotentialMatch ofBestMatchesGetOne(const path &fileNameToMatch,
        const std::vector<Mat> &descriptors,
        std::vector<PotentialMatch> &matches, SIFT &sifter) {

    BFMatcher matcher(NORM_L2, false);
    Mat imageToMatch = computeDescriptors(fileNameToMatch, sifter);


    /*
     * here we're essentially boosting the accuracy by re-examining our list
     * of potential matches a little more discriminately.  the result adds
     * about 10% of accuracy when we use a smaller query descriptor cap
     * (resulting in a faster initial search)
     */
    PotentialMatch bestMatch = matches[0];
    std::vector<int> numMatches;
    for (auto possibleMatch: matches) {

        if (possibleMatch.id < 0) {
            break;
        }

        Mat candidateDescriptor = descriptors[possibleMatch.id];
        MatchDetails details = compareImageToDesign(imageToMatch,
            candidateDescriptor, matcher, 0.75);

        if (details.numMatches > bestMatch.details.numMatches) {
            bestMatch.id = possibleMatch.id;
            bestMatch.details = details;
        }

        numMatches.push_back(possibleMatch.details.numMatches);
    }


    float mean = std::accumulate(numMatches.begin(), numMatches.end(), 0) / float(numMatches.size());
    float variance = 0;
    for (int num: numMatches) {
        variance += pow(mean - num, 2);
    }
    variance /= numMatches.size();
    float deviation = sqrt(variance);

    /*
     * these were computed roughly from the average number of standard
     * deviations the bestMatch is, for both correct matches and incorrect
     * matches from our test data.  in other words, for all correct test
     * matches, the average number of standard deviations the bestMatch was,
     * was ~20 (signifying a strong signal).  these should be tweaked as we
     * aggregate more test images
     */
    float goodMatch = 25.0;
    float shitMatch = 1.0;

    bestMatch.stdAway = (bestMatch.details.numMatches - mean) / deviation;
    bestMatch.confidence = std::max(std::min((bestMatch.stdAway - shitMatch)
        / (goodMatch - shitMatch), 1.0f), 0.0f);

    return bestMatch;
}


/*
 * this functor is contains parallelizable matching code.  it can be fed into
 * TBB's parallel_for or used serially, so long as the blocked_range passed
 * into operator() is correct
 */
class MatchFunctor {
public:
    MatchFunctor(const Mat &imageToMatch, const std::vector<Mat> &descriptors,
        float distanceRatioThreshold, std::vector<PotentialMatch> &results):
        imageToMatch(imageToMatch), descriptors(descriptors),
        distanceRatioThreshold(distanceRatioThreshold), results(results) {
    }

    void operator()(const tbb::blocked_range<size_t>& r) const {
        BFMatcher matcher(NORM_L2, false);

        double start = logging::timestamp();

        for (size_t i=r.begin(); i!=r.end(); i++) {
            Mat candidateDescriptor = descriptors[i];

            if (candidateDescriptor.cols == 0) {
                dlog(i << " had no descriptors, skipping", logging::LOW);
                continue;
            }

            results[i].id = i;
            results[i].details = compareImageToDesign(imageToMatch,
                candidateDescriptor, matcher, distanceRatioThreshold);
        }

        double elapsed = logging::timestamp() - start;
        float average = elapsed / r.size();

        dlog("performed " << r.size() << " comparisons, averaged " << average
            << " seconds per comparison", logging::LOW);
    }

private:
    const Mat &imageToMatch;
    const std::vector<Mat> &descriptors;
    float distanceRatioThreshold;
    std::vector<PotentialMatch> &results;
};



/*
 * returns a list of N (numBestMatches) PotentialMatches.  it's usually better
 * to use findBestMatch instead of this method directly, because findBestMatch
 * performs additional checks to increase the accuracy and return a single
 * match
 */
std::vector<PotentialMatch> findBestMatches(const path &fileNameToMatch,
        const std::vector<Mat> &descriptors, int numBestMatches,
        float distanceRatioThreshold, SIFT &sifter, bool multithreaded) {

    Mat imageToMatch = computeDescriptors(fileNameToMatch, sifter);


    std::vector<PotentialMatch> results(descriptors.size());
    MatchFunctor fn(imageToMatch, descriptors, distanceRatioThreshold, results);
    tbb::blocked_range<size_t> range(0, descriptors.size());

    if (multithreaded) {
        tbb::parallel_for(range, fn);
    }
    else {
        fn(range);
    }


    dlog("sorting and filtering to " << numBestMatches << " best matches", logging::HIGH);
    std::sort_heap(results.begin(), results.end(), std::greater<PotentialMatch>());

    std::vector<PotentialMatch> bestResults;
    for (int i=0; i<numBestMatches; i++) {
        bestResults.push_back(results[i]);
    }
    dlog("done sorting and filtering best matches", logging::HIGH);

    return bestResults;
}





void shutdown(int param) {
    dlog("caught signal " << param << ", shutting down", logging::HIGH);
    server.stop();
    exit(1);
}


/*
 * used for testing changes to our optimizations and tuning of SIFT parameters.
 * this just runs through our pre-classified test images and performs a best
 * match, measures the average matching time, and calculates a average accuracy
 */
void runTest(const path& designDir, const path &testImagesDir,
        const std::vector<Mat> &descriptors, int numBestMatches,
        float distanceRatioThreshold, SIFT &sifter, SIFT &refineSifter,
        bool multithreaded) {

    /*
     * perform the matching on all images in testImagesDir
     */
    double start = logging::timestamp();
    std::map<int, PotentialMatch> guesses;
    applyFunctionToImages(testImagesDir, [&](const path& filePath) {
        int correct = std::stoi(filePath.stem().string());
        MatchInfo guess = findBestMatch(filePath, descriptors, numBestMatches,
            distanceRatioThreshold, sifter, refineSifter, multithreaded);
        guesses[correct] = guess.match;
    }, 50);
    double elapsed = logging::timestamp() - start;


    /*
     * record how far away our best match is as well, so we can refine our
     * confidence calculations with more data later
     */
    std::vector<float> correctSTDAway;
    std::vector<float> incorrectSTDAway;

    /*
     * loop through and figure out how many we got correct, and how many we
     * didn't
     */
    int correctAnswers = 0;
    int testDesigns = guesses.size();
    for (auto guess: guesses) {
        if (guess.first == guess.second.id) {
            correctAnswers++;
            correctSTDAway.push_back(guess.second.stdAway);
        }
        else {
            dlog("bad guess: " << guess.first << " thought " << guess.second.id, logging::HIGH);
            incorrectSTDAway.push_back(guess.second.stdAway);
        }
    }

    dlog("avg match time " << (elapsed / testDesigns) << ", accuracy: "
        << (correctAnswers / float(testDesigns)), logging::HIGH);
}


/*
 * takes a directory of training designs and computes keypoints and descriptors
 * for all of those images
 */
void generateDescriptors(const path &imageDir, const path &outputDir,
        SIFT &sifter, bool skipExists) {
    if (!boost::filesystem::exists(outputDir)) {
        boost::filesystem::create_directories(outputDir);
    }
    applyFunctionToImages(imageDir, [&](const path& imagePath){
        generateKeypointsAndDescriptors(imagePath, outputDir, sifter, skipExists);
    }, 0);
}



int main(int argc, char** argv) {
    int port;
    int healthyThreshold;
    bool generateMode;
    bool testMode;
    bool singlethreaded;

    namespace opt = boost::program_options;
    opt::options_description desc("Options");
    desc.add_options()
        ("help", "help message")
        ("base", opt::value<std::string>(),
            "base directory of our data files")
        ("port", opt::value<int>(&port)->default_value(8080),
            "HTTP port to listen on")
        ("unhealthy", opt::value<int>(&healthyThreshold)->default_value(2),
            "the number of simultaneous matching requests at which the server becomes unhealthy")
        ("generate", opt::bool_switch(&generateMode),
            "generate descriptors")
        ("singlethreaded", opt::bool_switch(&singlethreaded),
            "don't parallelize matching with TBB")
        ("test", opt::bool_switch(&testMode),
            "run time and accuracy tests")
    ;
    opt::variables_map options;
    opt::store(opt::parse_command_line(argc, argv, desc), options);
    opt::notify(options);

    if (options.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }


    if (!options.count("base")) {
        std::cerr << "please specify a base directory with --base\n";
        return 1;
    }

    DATA_DIR = options["base"].as<std::string>();

    if (!boost::filesystem::exists(DATA_DIR)) {
        std::cerr << "base directory " << DATA_DIR << " doesn't exist!\n";
        return 1;
    }




    /*
     * a blur sigma of 3.0 seems to be the sweet spot between high accuracy
     * and low matching time
     */
    float sigma = 3.0;

    /*
     * after graphing the distribution of descriptors over all training images,
     * 3500 seems to be a good cutoff
     */
    int maxTrainDescriptors = 3500;

    /*
     * where our designs are held
     */
    path designsDir = DATA_DIR/"designs";
    /*
     * if you want to use tinier thumbnails to stream to clients, specify
     * another directory here
     */
    //path designThumbsDir = DATA_DIR/"design_thumbnails";
    path designThumbsDir = designsDir;

    std::stringstream descriptorRelativePath;
    descriptorRelativePath << "descriptors-" << maxTrainDescriptors << "-"
        << std::setprecision(2) << sigma;
    path descriptorDir = DATA_DIR/descriptorRelativePath.str();

    /*
     * where our testing images are held, for running accuracy tests on
     * optimizations.  the images in this directory have been pre-cropped and
     * flipped (if they were taken in a mirror), and the filename of the image
     * is the correct design
     */
    path testImagesDir = DATA_DIR/"test_images";


    /*
     * SIFT parameters
     */
    int octaves = 3;
    float contrastThreshold = 0.04;
    float edgeThreshold = 10;
    int numMatches = 80;
    float thresholdRatio = 0.75;

    /*
     * sifters used in various stages.  "sifter" is used for our initial
     * comparisons, and therefore uses a low number of features for speed.
     * "refineSifter" is a second pass that runs on the numMatches best matches
     * from "sifter"'s comparisons.  "generateSifter" is for generating our
     * descriptors from our initial design data, and is only used for
     * --generate
     */
    SIFT sifter(80, octaves, contrastThreshold, edgeThreshold, sigma);
    SIFT refineSifter(300, octaves, contrastThreshold, edgeThreshold, sigma);
    SIFT generateSifter(maxTrainDescriptors, octaves, contrastThreshold,
        edgeThreshold, sigma);

    /*
     * our mapping of product id to product info
     */
    MatchInfo::designInfoData = loadDesignInfoData(DATA_DIR/"prod_mapping.yaml");
    MatchInfo::designThumbsDir = designThumbsDir;

    if (generateMode) {
        generateDescriptors(designsDir, descriptorDir, generateSifter, true);
        return 0;
    }

    /*
     * preload our 6GB+ of image descriptors.  this will take around half a
     * minute or so.  they're used for all the image matching
     */
    std::vector<Mat> descriptors = preloadDescriptors(descriptorDir);


    if (testMode) {
        runTest(designsDir, testImagesDir, descriptors, numMatches, thresholdRatio,
            sifter, refineSifter, !singlethreaded);
        return 0;
    }

    /*
     * set the matcher our server should use to match images.  it's just a
     * closure with some preset defaults
     */
    server.setMatcher([&descriptors, &sifter, &refineSifter, numMatches,
        thresholdRatio, singlethreaded](const path &imagePath)->MatchInfo{
        MatchInfo info = findBestMatch(imagePath, descriptors, numMatches,
                thresholdRatio, sifter,	refineSifter, !singlethreaded);
        return info;
    });

    server.setHealthyThreshold(healthyThreshold);

    /*
     * set up our signal handler and launch the web server
     */
    signal(SIGTERM, shutdown);
    server.serve(port);


    while (true) {
        sleep(1);
    }


    return 0;
}
