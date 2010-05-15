/* add your code here */

#ifndef _IOXBOXPARTITIONSCHEME_H
#define _IOXBOXPARTITIONSCHEME_H

#include <IOKit/IOTypes.h>

#define kIOXboxPartitionSchemeClass "IOXboxPartitionScheme"

#define N_XBOX_PARTITIONS 4

#define CACHE_PART_START					0x000080000LLU
#define GAME_CACHE_PART_START				0x080080000LLU
#define XBOX_BACKWARDS_COMPAT_PART_START	0x120EB0000LLU
#define MAIN_PART_START						0x130EB0000LLU

#define HEADER_PADDING_LENGTH 8192
#define PLAINTEXT_HD_INFO_LENGTH 68
#define ENCRYPTED_HD_INFO_LENGTH (20+4+256)
#define PNG_LENGTH 2754

struct xbox_header 
{
	UInt8	junk[HEADER_PADDING_LENGTH];
	char	hdInfo[PLAINTEXT_HD_INFO_LENGTH];
	UInt8	encHdInfo[ENCRYPTED_HD_INFO_LENGTH];
	UInt16	pngLength;
	UInt8	pngData[PNG_LENGTH];
};

#define FATX_SUPERBLOCK_HEADER_LENGTH 4
#define FATX_SUPERBLOCK_PADDING_LENGTH (4+4078)

struct xbox_fatx_superblock
{
	char	header[FATX_SUPERBLOCK_HEADER_LENGTH];
	UInt32	volumeId;
	UInt32	clusterSz;
	UInt16	nFat;
	UInt8	junk[FATX_SUPERBLOCK_PADDING_LENGTH];
};

#ifdef KERNEL
#ifdef __cplusplus

#include <IOKit/storage/IOPartitionScheme.h>

class IOXboxPartitionScheme : public IOPartitionScheme
{
	OSDeclareDefaultStructors(IOXboxPartitionScheme);
	
protected:	
	virtual void free(void);

public:
	virtual bool init(OSDictionary * properties = 0);
	virtual IOService * probe(IOService * provider, SInt32 * score);
	virtual bool start(IOService * provider);
	virtual void stop(IOService * provider);
	
private:
	OSSet * _partitions;
	OSSet * scan(SInt32 * score);
	bool	isFatxPartition(struct xbox_fatx_superblock *);
};

#endif /* __cplusplus */
#endif /* KERNEL */
#endif /* !_IOXBOXPARTITIONSCHEME_H */