//Download by http://www.NewXing.com
#ifndef CRYPTOPP_THREEWAY_H
#define CRYPTOPP_THREEWAY_H

#include "cryptlib.h"
#include "misc.h"

NAMESPACE_BEGIN(CryptoPP)

class ThreeWayEncryption : public BlockTransformation
{
public:
	enum {KEYLENGTH=12, BLOCKSIZE=12, ROUNDS=11};

	ThreeWayEncryption(const byte *userKey, unsigned rounds=ROUNDS);
	~ThreeWayEncryption();

	void ProcessBlock(const byte *inBlock, byte * outBlock) const;
	void ProcessBlock(byte * inoutBlock) const
		{ThreeWayEncryption::ProcessBlock(inoutBlock, inoutBlock);}

	unsigned int BlockSize() const {return BLOCKSIZE;}

private:
	word32 k[3];
	unsigned int rounds;
	SecBlock<word32> rc;
};

class ThreeWayDecryption : public BlockTransformation
{
public:
	enum {KEYLENGTH=12, BLOCKSIZE=12, ROUNDS=11};

	ThreeWayDecryption(const byte *userKey, unsigned rounds=ROUNDS);
	~ThreeWayDecryption();

	void ProcessBlock(const byte *inBlock, byte * outBlock) const;
	void ProcessBlock(byte * inoutBlock) const
		{ThreeWayDecryption::ProcessBlock(inoutBlock, inoutBlock);}

	unsigned int BlockSize() const {return BLOCKSIZE;}

private:
	word32 k[3];
	unsigned int rounds;
	SecBlock<word32> rc;
};

NAMESPACE_END

#endif
