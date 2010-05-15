#include <IOKit/assert.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "IOXboxPartitionScheme.h"

#define super IOPartitionScheme

OSDefineMetaClassAndStructors(IOXboxPartitionScheme, IOPartitionScheme);

bool IOXboxPartitionScheme::init(OSDictionary * properties)
{
	if ( super::init(properties) == false ) return false;
	
	_partitions = 0;
	
	return true;
}

void IOXboxPartitionScheme::free()
{
	if (_partitions) {
		_partitions->release();
	}
	
	super::free();
}

IOService * IOXboxPartitionScheme::probe(IOService * provider, SInt32 * score)
{
	assert(OSDynamicCast(IOMedia, provider));
	
	if ( super::probe(provider, score) == 0 ) {
		return 0;
	}
	
	_partitions = scan(score);

	return _partitions ? this : 0;
}

bool IOXboxPartitionScheme::start(IOService * provider)
{
	IOMedia *		partition;
	OSIterator *	partitionIter;
	
	assert(_partitions);

	if ( super::start(provider) == false ) return false;

	partitionIter = OSCollectionIterator::withCollection(_partitions);
	if ( partitionIter == 0 ) return false;

	while ( (partition = (IOMedia *) partitionIter->getNextObject()) ) {
		if ( (partition->attach(this)) ) {
		    attachMediaObjectToDeviceTree(partition);
			partition->registerService();
		}
	}
	partitionIter->release();
	return true;
}

void IOXboxPartitionScheme::stop(IOService * provider)
{
	IOMedia *		partition;
	OSIterator *	partitionIter;

	assert(_partitions);

	partitionIter = OSCollectionIterator::withCollection(_partitions);
	
	if ( partitionIter ) {
		while ( (partition = (IOMedia *) partitionIter->getNextObject()) ) {
			detachMediaObjectFromDeviceTree(partition);
		}
	    partitionIter->release();
	}
	super::stop(provider);
}

OSSet * IOXboxPartitionScheme::scan(SInt32 * score)
{
	IOBufferMemoryDescriptor *			buffer;
	UInt32								bufferSize;
	struct xbox_fatx_superblock *		fatxSuperblock;
	IOMedia *							media = getProvider();
	UInt64								blockSize = media->getPreferredBlockSize();
	bool								mediaIsOpen = false;
	OSSet *								partitions;
	IOMedia *							partition;
	
	if ( media->isFormatted() == false ) goto err;
	
	bufferSize	= IORound(sizeof(xbox_fatx_superblock), blockSize);
	buffer		= IOBufferMemoryDescriptor::withCapacity(bufferSize, 
														 kIODirectionIn);
	
	if ( buffer == 0) goto err;
	
	partitions = OSSet::withCapacity(N_XBOX_PARTITIONS);
	if ( partitions == 0) goto err;
	
	mediaIsOpen = open(this, 0, kIOStorageAccessReader);
	if ( mediaIsOpen == false) goto err;
	
	IOLog("Checking cache partition.\n");
	if ( media->read(this, CACHE_PART_START, buffer) != kIOReturnSuccess ) goto err;
	fatxSuperblock = (struct xbox_fatx_superblock *) buffer->getBytesNoCopy();
	if (isFatxPartition(fatxSuperblock) == false) goto err;

	/*
	IOLog("Checking xbox backwards compat partition.\n");
	if ( media->read(this, XBOX_BACKWARDS_COMPAT_PART_START, buffer) != kIOReturnSuccess ) goto err;
	fatxSuperblock = (struct xbox_fatx_superblock *) buffer->getBytesNoCopy();
	if (isFatxPartition(fatxSuperblock) == false) goto err;
	*/
	
	IOLog("Checking main partition.\n");
	if ( media->read(this, MAIN_PART_START, buffer) != kIOReturnSuccess ) goto err;
	fatxSuperblock = (struct xbox_fatx_superblock *) buffer->getBytesNoCopy();
	if (isFatxPartition(fatxSuperblock) == false) goto err;
	
	IOLog("Disk is an xbox disk! Setuping up partitions.\n");
	
	// Cache Partition
	partition = new IOMedia;
	if ( partition->init(CACHE_PART_START, 
						 GAME_CACHE_PART_START - CACHE_PART_START, 
						 blockSize, 
						 media->getAttributes(), 
						 false, 
						 media->isWritable(), 
						 "FATX") ) {
		partition->setName("Cache Partition");
		partition->setLocation("0");
	} else {
		goto err;
	}
	partitions->setObject(partition);
	
	// Game Cache Partition
	partition = new IOMedia;
	if ( partition->init(GAME_CACHE_PART_START, 
						 XBOX_BACKWARDS_COMPAT_PART_START - GAME_CACHE_PART_START, 
						 blockSize, 
						 media->getAttributes(), 
						 false, 
						 media->isWritable(), 
						 "STFC") ) {
		partition->setName("Game Cache Partition");
		partition->setLocation("1");
	} else {
		partition->release();
		goto err;
	}
	partitions->setObject(partition);

	// Xbox Backwards Compatibility drive
	partition = new IOMedia;
	if ( partition->init(XBOX_BACKWARDS_COMPAT_PART_START, 
						 MAIN_PART_START - XBOX_BACKWARDS_COMPAT_PART_START, 
						 blockSize, 
						 media->getAttributes(), 
						 false, 
						 media->isWritable(), 
						 "FATX") ) {
		partition->setName("Xbox Backwards Compatibility drive");
		partition->setLocation("2");
	} else {
		partition->release();
		goto err;
	}
	partitions->setObject(partition);
	
	// Main Xbox360 Partition
	partition = new IOMedia;
	if ( partition->init(MAIN_PART_START, 
						 media->getSize() - MAIN_PART_START, 
						 blockSize, 
						 media->getAttributes(), 
						 false, 
						 media->isWritable(), 
						 "FATX") ) {
		partition->setName("Main Xbox360 Partition");
		partition->setLocation("3");
	} else {
		partition->release();
		goto err;
	}
	partitions->setObject(partition);
	
	close(this);
	buffer->release();
	return partitions;
	
err:
	if (mediaIsOpen) close(this);
	if (partitions) partitions->release();
	if (buffer) buffer->release();
	return 0;
}

bool IOXboxPartitionScheme::isFatxPartition(struct xbox_fatx_superblock * superblock)
{
	if (superblock->header[0] == 'X' && 
		superblock->header[1] == 'T' &&
		superblock->header[2] == 'A' &&
		superblock->header[3] == 'F') {
		return true;
	}
	return false;
}