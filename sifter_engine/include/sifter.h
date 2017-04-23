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

#ifndef SIFTER_H_
#define SIFTER_H_

#include <map>
#include <memory>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/nonfree/features2d.hpp>

#include <boost/filesystem.hpp>







using namespace cv;
typedef boost::filesystem::path path;
typedef boost::filesystem::directory_iterator dirIt;
typedef boost::filesystem::recursive_directory_iterator recDirIt;


struct DesignInfo {
    int id;
    std::string artistName;
    std::string artistUrl;
    std::string dateAdded;
    std::string title;
};

struct MatchDetails {
    int numMatches = 0;
    float totalDistance = 0;
    float averageDistance = 0;
};

struct PotentialMatch {
    PotentialMatch();
    bool operator<(const PotentialMatch &m2);
    bool operator<(int minMatches);
    bool operator>=(const PotentialMatch &m2);
    bool operator>(const PotentialMatch &m2);

    friend std::ostream &operator<<(std::ostream &, const PotentialMatch &);

    int id = -1;
    float confidence = 0;
    float stdAway = 0;
    MatchDetails details;
};

struct MatchInfo {
    static std::map<int, DesignInfo> designInfoData;
    static path designThumbsDir;

    MatchInfo(const PotentialMatch &match, float elapsed);

    std::string designUrl;
    DesignInfo design;
    PotentialMatch match;
    float elapsed;
    std::string thumbnail;
    int width;
    int height;
    std::string json();
};



std::vector<PotentialMatch> findBestMatches(const path &fileNameToMatch,
    const std::vector<Mat> &descriptors, int numBestMatches,
    float distanceRatioThreshold, SIFT &sifter, bool multithreaded);

PotentialMatch ofBestMatchesGetOne(const path &fileNameToMatch,
    const std::vector<Mat> &descriptors,
    std::vector<PotentialMatch> &matches, SIFT &sifter);


#endif /* SIFTER_H_ */
