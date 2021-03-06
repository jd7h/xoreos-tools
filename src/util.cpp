/* xoreos-tools - Tools to help with xoreos development
 *
 * xoreos-tools is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos-tools is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos-tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos-tools. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  General tool utility functions.
 */

#include "src/common/error.h"
#include "src/common/ustring.h"
#include "src/common/platform.h"
#include "src/common/readstream.h"
#include "src/common/readfile.h"
#include "src/common/writefile.h"
#include "src/common/stdinstream.h"
#include "src/common/stdoutstream.h"

#include "src/util.h"

void initPlatform() {
	try {
		Common::Platform::init();
	} catch (...) {
		Common::exceptionDispatcherError("Failed to initialize the low-level platform-specific subsytem");
	}
}

void dumpStream(Common::SeekableReadStream &stream, const Common::UString &fileName) {
	Common::WriteFile file;
	if (!file.open(fileName))
		throw Common::Exception(Common::kOpenError);

	file.writeStream(stream);
	file.flush();

	file.close();
}

bool isFileStd(const Common::UString &file) {
	return file.empty() || file == "-";
}

Common::WriteStream *openFileOrStdOut(const Common::UString &file) {
	if (!isFileStd(file))
		return new Common::WriteFile(file);

	return new Common::StdOutStream;
}

Common::ReadStream *openFileOrStdIn(const Common::UString &file) {
	if (!isFileStd(file))
		return new Common::ReadFile(file);

	return new Common::StdInStream;
}
