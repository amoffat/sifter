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

#ifndef LOGGING_HPP_
#define LOGGING_HPP_

#include <iostream>
#include <iomanip>

#include <sys/time.h>
#include <cstddef>


namespace logging {
	extern double lastLog;
	enum priority {HIGH, MEDIUM, LOW};
	extern priority currentPriority;

	inline double timestamp() {
		timeval tv;
		gettimeofday(&tv, NULL);
		return tv.tv_sec + (tv.tv_usec / 1000000.0);
	}
}

#ifdef LOGGING
#define dlog(msg, p)\
do {\
	if (p <= logging::currentPriority) {\
		double ts = logging::timestamp();\
		std::cout << "(" << getpid() << ") "\
			<< std::fixed << std::setprecision(6) << ts \
			<< " "	<< (logging::lastLog == 0 ? 0 : ts - logging::lastLog)\
			<< " " << __FILE__ << " line " << __LINE__\
			<< ": ";\
		std::cout << msg << std::endl;\
		std::flush(std::cout);\
		logging::lastLog = ts;\
	}\
} while(0)
#else
#define dlog(msg, p)
#endif



// alog = always log
#define alog(msg, p)\
do {\
	if (p <= logging::currentPriority) {\
		double ts = logging::timestamp();\
		std::cout << "(" << getpid() << ") " << std::fixed << std::setprecision(6)\
			<< ts << " "	<< (logging::lastLog == 0 ? 0 : ts - logging::lastLog) << ": ";\
		std::cout << msg << std::endl;\
		std::flush(std::cout);\
		logging::lastLog = ts;\
	}\
} while(0)


#endif /* LOGGING_HPP_ */
