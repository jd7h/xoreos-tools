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
 *  TXB (another one of BioWare's own texture formats) loading.
 */

#ifndef IMAGES_TXB_H
#define IMAGES_TXB_H

#include "src/images/decoder.h"

namespace Common {
	class SeekableReadStream;
}

namespace Images {

/** Another one of BioWare's own texture formats, TXB. */
class TXB : public Decoder {
public:
	TXB(Common::SeekableReadStream &txb);
	~TXB();

	/** Return the enclosed TXI data. */
	Common::SeekableReadStream *getTXI() const;

private:
	size_t _dataSize;

	byte  *_txiData;
	size_t _txiDataSize;

	// Loading helpers
	void load(Common::SeekableReadStream &txb);
	void readHeader(Common::SeekableReadStream &txb, bool &needDeSwizzle);
	void readData(Common::SeekableReadStream &txb, bool needDeSwizzle);
	void readTXIData(Common::SeekableReadStream &txb);

	static void deSwizzle(byte *dst, const byte *src, uint32 width, uint32 height);
};

} // End of namespace Images

#endif // IMAGES_TXB_H
