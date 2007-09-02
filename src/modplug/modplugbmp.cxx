/* Modplug XMMS Plugin
 * Authors: Kenton Varda <temporal@gauge3d.org>
 *
 * This source code is public domain.
 */

#include <fstream>
#include <unistd.h>
#include <math.h>

#include "modplugbmp.h"
#include "stdafx.h"
#include "sndfile.h"
#include "stddefs.h"
#include "archive/open.h"
extern "C" {
#include "audacious/configdb.h"
#include "audacious/output.h"
#include "audacious/tuple.h"
#include "audacious/tuple_formatter.h"
#include "audacious/vfs.h"
}

static char* format_and_free_ti( Tuple* ti, int* length )
{
        char* result = tuple_formatter_make_title_string(ti, get_gentitle_format());
        if ( result )
                *length = tuple_get_int(ti, "length");
        tuple_free((void *) ti);

        return result;
}

// ModplugXMMS member functions ===============================

// operations ----------------------------------------
ModplugXMMS::ModplugXMMS()
{
	mSoundFile = new CSoundFile;
	mOutPlug = NULL;
}
ModplugXMMS::~ModplugXMMS()
{
	delete mSoundFile;
}

ModplugXMMS::Settings::Settings()
{
	mSurround       = true;
	mOversamp       = true;
	mReverb         = false;
	mMegabass       = false;
	mNoiseReduction = true;
	mVolumeRamp     = true;
	mFastinfo       = true;
	mUseFilename    = false;
	mGrabAmigaMOD   = true;

	mChannels       = 2;
	mFrequency      = 44100;
	mBits           = 16;
	mResamplingMode = SRCMODE_POLYPHASE;

	mReverbDepth    = 30;
	mReverbDelay    = 100;
	mBassAmount     = 40;
	mBassRange      = 30;
	mSurroundDepth  = 20;
	mSurroundDelay  = 20;
	
	mPreamp         = false;
	mPreampLevel    = 0.0f;
	
	mLoopCount      = 0;   //don't loop
}

void ModplugXMMS::Init(void)
{
	ConfigDb *db;
	
	db = bmp_cfg_db_open();

	bmp_cfg_db_get_bool(db,"modplug","Surround", &mModProps.mSurround);
        bmp_cfg_db_get_bool(db,"modplug","Oversampling", &mModProps.mOversamp);
        bmp_cfg_db_get_bool(db,"modplug","Megabass", &mModProps.mMegabass);
        bmp_cfg_db_get_bool(db,"modplug","NoiseReduction", &mModProps.mNoiseReduction);
        bmp_cfg_db_get_bool(db,"modplug","VolumeRamp", &mModProps.mVolumeRamp);
        bmp_cfg_db_get_bool(db,"modplug","Reverb", &mModProps.mReverb);
        bmp_cfg_db_get_bool(db,"modplug","FastInfo", &mModProps.mFastinfo);
        bmp_cfg_db_get_bool(db,"modplug","UseFileName", &mModProps.mUseFilename);
        bmp_cfg_db_get_bool(db,"modplug","GrabAmigaMOD", &mModProps.mGrabAmigaMOD);
        bmp_cfg_db_get_bool(db,"modplug","PreAmp", &mModProps.mPreamp);
        bmp_cfg_db_get_float(db,"modplug","PreAmpLevel", &mModProps.mPreampLevel);
        bmp_cfg_db_get_int(db,"modplug", "Channels", &mModProps.mChannels);
        bmp_cfg_db_get_int(db,"modplug", "Bits", &mModProps.mBits);
        bmp_cfg_db_get_int(db,"modplug", "Frequency", &mModProps.mFrequency);
        bmp_cfg_db_get_int(db,"modplug", "ResamplineMode", &mModProps.mResamplingMode);
        bmp_cfg_db_get_int(db,"modplug", "ReverbDepth", &mModProps.mReverbDepth);
        bmp_cfg_db_get_int(db,"modplug", "ReverbDelay", &mModProps.mReverbDelay);
        bmp_cfg_db_get_int(db,"modplug", "BassAmount", &mModProps.mBassAmount);
        bmp_cfg_db_get_int(db,"modplug", "BassRange", &mModProps.mBassRange);
        bmp_cfg_db_get_int(db,"modplug", "SurroundDepth", &mModProps.mSurroundDepth);
        bmp_cfg_db_get_int(db,"modplug", "SurroundDelay", &mModProps.mSurroundDelay);
        bmp_cfg_db_get_int(db,"modplug", "LoopCount", &mModProps.mLoopCount);

	bmp_cfg_db_close(db);
}

bool ModplugXMMS::CanPlayFileFromVFS(const string& aFilename, VFSFile *file)
{
	string lExt;
	uint32 lPos;

	gchar magic[4];

	vfs_fread(magic, 1, 4, file);
	if (!memcmp(magic, UMX_MAGIC, 4))
		return true;
	if (!memcmp(magic, XM_MAGIC, 4))
		return true;
	if (!memcmp(magic, M669_MAGIC, 2))
		return true;
	if (!memcmp(magic, IT_MAGIC, 4))
		return true;
	if (!memcmp(magic, MTM_MAGIC, 4))
		return true;
	if (!memcmp(magic, PSM_MAGIC, 4))
		return true;

	vfs_fseek(file, 44, SEEK_SET);
	vfs_fread(magic, 1, 4, file);
	if (!memcmp(magic, S3M_MAGIC, 4))
		return true;
	vfs_fseek(file, 1080, SEEK_SET);
	vfs_fread(magic, 1, 4, file);
	if (!memcmp(magic, MOD_MAGIC_FASTTRACKER6, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_FASTTRACKER8, 4))
		return true;
	if(mModProps.mGrabAmigaMOD) {
	if (!memcmp(magic, MOD_MAGIC_PROTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_PROTRACKER4X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_NOISETRACKER, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER8, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER4X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_STARTRACKER8X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_FASTTRACKER4, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_OKTALYZER8, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_OKTALYZER8X, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_TAKETRACKER16, 4))
		return true;
	if (!memcmp(magic, MOD_MAGIC_TAKETRACKER32, 4))
		return true;
	} /* end of if(mModProps.mGrabAmigaMOD) */

	/* We didn't find the magic bytes, fall back to extension check */
	lPos = aFilename.find_last_of('.');
	if((int)lPos == -1)
		return false;
	lExt = aFilename.substr(lPos);
	for(uint32 i = 0; i < lExt.length(); i++)
		lExt[i] = tolower(lExt[i]);

	if (lExt == ".amf")
		return true;
	if (lExt == ".ams")
		return true;
	if (lExt == ".dbm")
		return true;
	if (lExt == ".dbf")
		return true;
	if (lExt == ".dsm")
		return true;
	if (lExt == ".far")
		return true;
	if (lExt == ".mdl")
		return true;
	if (lExt == ".stm")
		return true;
	if (lExt == ".ult")
		return true;
	if (lExt == ".j2b")
		return true;
	if (lExt == ".mt2")
		return true;

	if (lExt == ".mdz")
		return true;
	if (lExt == ".mdr")
		return true;
	if (lExt == ".mdgz")
		return true;
	if (lExt == ".mdbz")
		return true;
	if (lExt == ".s3z")
		return true;
	if (lExt == ".s3r")
		return true;
	if (lExt == ".s3gz")
		return true;
	if (lExt == ".xmz")
		return true;
	if (lExt == ".xmr")
		return true;
	if (lExt == ".xmgz")
		return true;
	if (lExt == ".itz")
		return true;
	if (lExt == ".itr")
		return true;
	if (lExt == ".itgz")
		return true;
	if (lExt == ".dmf")
		return true;
	
	if (lExt == ".zip")
		return ContainsMod(aFilename);
	if (lExt == ".gz")
		return ContainsMod(aFilename);
	if (lExt == ".bz2")
		return ContainsMod(aFilename);

	return false;
}

void ModplugXMMS::PlayLoop()
{
	uint32 lLength;
	//the user might change the number of channels while playing.
	// we don't want this to take effect until we are done!
	uint8 lChannels = mModProps.mChannels;

	while(!mStopped)
	{
		if(!(lLength = mSoundFile->Read(
				mBuffer,
				mBufSize)))
		{
			//no more to play.  Wait for output to finish and then stop.
			while((mOutPlug->buffer_playing())
			   && (!mStopped))
				usleep(10000);
			break;
		}
		
		if(mModProps.mPreamp)
		{
			//apply preamp
			if(mModProps.mBits == 16)
			{
				uint n = mBufSize >> 1;
				for(uint i = 0; i < n; i++) {
					short old = ((short*)mBuffer)[i];
					((short*)mBuffer)[i] *= (short int)mPreampFactor;
					// detect overflow and clip!
					if ((old & 0x8000) != 
					 (((short*)mBuffer)[i] & 0x8000))
					  ((short*)mBuffer)[i] = old | 0x7FFF;
						
				}
			}
			else
			{
				for(uint i = 0; i < mBufSize; i++) {
					uchar old = ((uchar*)mBuffer)[i];
					((uchar*)mBuffer)[i] *= (short int)mPreampFactor;
					// detect overflow and clip!
					if ((old & 0x80) != 
					 (((uchar*)mBuffer)[i] & 0x80))
					  ((uchar*)mBuffer)[i] = old | 0x7F;
				}
			}
		}
		
		if(mStopped)
			break;
	
		//wait for buffer space to free up.
		while(((mOutPlug->buffer_free()
		    < (int)mBufSize))
		   && (!mStopped))
			usleep(10000);
			
		if(mStopped)
			break;
		
		produce_audio
		(
			mPlayed,
			mFormat,
			lChannels,
			mBufSize,
			mBuffer,
			NULL
		);

		mPlayed += mBufTime;
	}

//	mOutPlug->flush(0);
	mOutPlug->close_audio();

	//Unload the file
	mSoundFile->Destroy();
	delete mArchive;

	if (mBuffer)
	{
		delete [] mBuffer;
		mBuffer = NULL;
	}

	mPaused = false;
	mStopped = true;
}

void ModplugXMMS::PlayFile(const string& aFilename, InputPlayback *ipb)
{
	int32 aLength;
	char *aModName;
	mStopped = true;
	mPaused = false;
	
	//open and mmap the file
	mArchive = OpenArchive(aFilename);
	if(mArchive->Size() == 0)
	{
		delete mArchive;
		return;
	}
	
	if (mBuffer)
		delete [] mBuffer;
	
	//find buftime to get approx. 512 samples/block
	mBufTime = 512000 / mModProps.mFrequency + 1;

	mBufSize = mBufTime;
	mBufSize *= mModProps.mFrequency;
	mBufSize /= 1000;    //milliseconds
	mBufSize *= mModProps.mChannels;
	mBufSize *= mModProps.mBits / 8;

	mBuffer = new uchar[mBufSize];
	if(!mBuffer)
		return;             //out of memory!

	CSoundFile::SetWaveConfig
	(
		mModProps.mFrequency,
		mModProps.mBits,
		mModProps.mChannels
	);
	CSoundFile::SetWaveConfigEx
	(
		mModProps.mSurround,
		!mModProps.mOversamp,
		mModProps.mReverb,
		true,
		mModProps.mMegabass,
		mModProps.mNoiseReduction,
		false
	);
	
	// [Reverb level 0(quiet)-100(loud)], [delay in ms, usually 40-200ms]
	if(mModProps.mReverb)
	{
		CSoundFile::SetReverbParameters
		(
			mModProps.mReverbDepth,
			mModProps.mReverbDelay
		);
	}
	// [XBass level 0(quiet)-100(loud)], [cutoff in Hz 10-100]
	if(mModProps.mMegabass)
	{
		CSoundFile::SetXBassParameters
		(
			mModProps.mBassAmount,
			mModProps.mBassRange
		);
	}
	// [Surround level 0(quiet)-100(heavy)] [delay in ms, usually 5-40ms]
	if(mModProps.mSurround)
	{
		CSoundFile::SetSurroundParameters
		(
			mModProps.mSurroundDepth,
			mModProps.mSurroundDelay
		);
	}
	CSoundFile::SetResamplingMode(mModProps.mResamplingMode);
	mSoundFile->SetRepeatCount(mModProps.mLoopCount);
	mPreampFactor = exp(mModProps.mPreampLevel);
	
	mPaused = false;
	mStopped = false;

	mSoundFile->Create
	(
		(uchar*)mArchive->Map(),
		mArchive->Size()
	);
	mPlayed = 0;

        Tuple* ti = GetSongTuple( aFilename );
        if ( ti )
                aModName = format_and_free_ti( ti, &aLength );

	mInPlug->set_info
	(
		aModName,
		aLength,
		mSoundFile->GetNumChannels() * 1000,
		mModProps.mFrequency,
		mModProps.mChannels
	);

	mStopped = mPaused = false;

	if(mModProps.mBits == 16)
		mFormat = FMT_S16_NE;
	else
		mFormat = FMT_U8;

	mOutPlug->open_audio
	(
		mFormat,
		mModProps.mFrequency,
		mModProps.mChannels
	);

	mDecodeThread = g_thread_self();
	ipb->set_pb_ready(ipb);
	this->PlayLoop();
}

void ModplugXMMS::Stop(void)
{
	if(mStopped)
		return;

	mStopped = true;
	mPaused = false;
	
	g_thread_join(mDecodeThread);
}

void ModplugXMMS::Pause(bool aPaused)
{
	if(aPaused)
		mPaused = true;
	else
		mPaused = false;
	
	mOutPlug->pause(aPaused);
}

void ModplugXMMS::Seek(float32 aTime)
{
	uint32  lMax;
	uint32  lMaxtime;
	float32 lPostime;
	
	if(aTime > (lMaxtime = mSoundFile->GetSongTime()))
		aTime = lMaxtime;
	lMax = mSoundFile->GetMaxPosition();
	lPostime = float(lMax) / lMaxtime;

	mSoundFile->SetCurrentPos(int(aTime * lPostime));

	mOutPlug->flush(int(aTime * 1000));
	mPlayed = uint32(aTime * 1000);
}

float32 ModplugXMMS::GetTime(void)
{
	if ((mStopped) || (!mOutPlug))
		return -1;
	return (float32)mOutPlug->output_time() / 1000;
}

Tuple* ModplugXMMS::GetSongTuple(const string& aFilename)
{
	CSoundFile* lSoundFile;
	Archive* lArchive;
	
	//open and mmap the file
        lArchive = OpenArchive(aFilename);
        if(lArchive->Size() == 0)
        {
                delete lArchive;
                return NULL;
        }

	Tuple *ti = tuple_new_from_filename(aFilename.c_str());
	lSoundFile = new CSoundFile;
	lSoundFile->Create((uchar*)lArchive->Map(), lArchive->Size());
	switch(lSoundFile->GetType())
        {
	case MOD_TYPE_MOD:
		tuple_associate_string(ti, "codec", "ProTracker");
		break;
	case MOD_TYPE_S3M:
		tuple_associate_string(ti, "codec", "Scream Tracker 3");
		break;
	case MOD_TYPE_XM:
		tuple_associate_string(ti, "codec", "Fast Tracker 2");
		break;
	case MOD_TYPE_IT:
		tuple_associate_string(ti, "codec", "Impulse Tracker");
		break;
	case MOD_TYPE_MED:
		tuple_associate_string(ti, "codec", "OctaMed");
		break;
	case MOD_TYPE_MTM:
		tuple_associate_string(ti, "codec", "MultiTracker Module");
		break;
	case MOD_TYPE_669:
		tuple_associate_string(ti, "codec", "669 Composer / UNIS 669");
		break;
	case MOD_TYPE_ULT:
		tuple_associate_string(ti, "codec", "Ultra Tracker");
		break;
	case MOD_TYPE_STM:
		tuple_associate_string(ti, "codec", "Scream Tracker");
		break;
	case MOD_TYPE_FAR:
		tuple_associate_string(ti, "codec", "Farandole");
		break;
	case MOD_TYPE_AMF:
		tuple_associate_string(ti, "codec", "ASYLUM Music Format");
		break;
	case MOD_TYPE_AMS:
		tuple_associate_string(ti, "codec", "AMS module");
		break;
	case MOD_TYPE_DSM:
		tuple_associate_string(ti, "codec", "DSIK Internal Format");
		break;
	case MOD_TYPE_MDL:
		tuple_associate_string(ti, "codec", "DigiTracker");
		break;
	case MOD_TYPE_OKT:
		tuple_associate_string(ti, "codec", "Oktalyzer");
		break;
	case MOD_TYPE_DMF:
		tuple_associate_string(ti, "codec", "Delusion Digital Music Fileformat (X-Tracker)");
		break;
	case MOD_TYPE_PTM:
		tuple_associate_string(ti, "codec", "PolyTracker");
		break;
	case MOD_TYPE_DBM:
		tuple_associate_string(ti, "codec", "DigiBooster Pro");
		break;
	case MOD_TYPE_MT2:
		tuple_associate_string(ti, "codec", "MadTracker 2");
		break;
	case MOD_TYPE_AMF0:
		tuple_associate_string(ti, "codec", "AMF0");
		break;
	case MOD_TYPE_PSM:
		tuple_associate_string(ti, "codec", "Protracker Studio Module");
		break;
	default:
		tuple_associate_string(ti, "codec", "ModPlug unknown");
		break;
	}
	tuple_associate_string(ti, "quality", "sequenced");
	tuple_associate_string(ti, "title", lSoundFile->GetTitle());
	tuple_associate_int(ti, "length", lSoundFile->GetSongTime() * 1000);
	
	//unload the file
	lSoundFile->Destroy();
	delete lSoundFile;
	delete lArchive;
	return ti;
}

void ModplugXMMS::SetInputPlugin(InputPlugin& aInPlugin)
{
	mInPlug = &aInPlugin;
}
void ModplugXMMS::SetOutputPlugin(OutputPlugin& aOutPlugin)
{
	mOutPlug = &aOutPlugin;
}

const ModplugXMMS::Settings& ModplugXMMS::GetModProps()
{
	return mModProps;
}

const char* ModplugXMMS::Bool2OnOff(bool aValue)
{
	if(aValue)
		return "on";
	else
		return "off";
}

void ModplugXMMS::SetModProps(const Settings& aModProps)
{
	ConfigDb *db;
	mModProps = aModProps;

	// [Reverb level 0(quiet)-100(loud)], [delay in ms, usually 40-200ms]
	if(mModProps.mReverb)
	{
		CSoundFile::SetReverbParameters
		(
			mModProps.mReverbDepth,
			mModProps.mReverbDelay
		);
	}
	// [XBass level 0(quiet)-100(loud)], [cutoff in Hz 10-100]
	if(mModProps.mMegabass)
	{
		CSoundFile::SetXBassParameters
		(
			mModProps.mBassAmount,
			mModProps.mBassRange
		);
	}
	else //modplug seems to ignore the SetWaveConfigEx() setting for bass boost
	{
		CSoundFile::SetXBassParameters
		(
			0,
			0
		);
	}
	// [Surround level 0(quiet)-100(heavy)] [delay in ms, usually 5-40ms]
	if(mModProps.mSurround)
	{
		CSoundFile::SetSurroundParameters
		(
			mModProps.mSurroundDepth,
			mModProps.mSurroundDelay
		);
	}
	CSoundFile::SetWaveConfigEx
	(
		mModProps.mSurround,
		!mModProps.mOversamp,
		mModProps.mReverb,
		true,
		mModProps.mMegabass,
		mModProps.mNoiseReduction,
		false
	);
	CSoundFile::SetResamplingMode(mModProps.mResamplingMode);
	mPreampFactor = exp(mModProps.mPreampLevel);

	db = bmp_cfg_db_open();

	bmp_cfg_db_set_bool(db,"modplug","Surround", mModProps.mSurround);
        bmp_cfg_db_set_bool(db,"modplug","Oversampling", mModProps.mOversamp);
        bmp_cfg_db_set_bool(db,"modplug","Megabass", mModProps.mMegabass);
        bmp_cfg_db_set_bool(db,"modplug","NoiseReduction", mModProps.mNoiseReduction);
        bmp_cfg_db_set_bool(db,"modplug","VolumeRamp", mModProps.mVolumeRamp);
        bmp_cfg_db_set_bool(db,"modplug","Reverb", mModProps.mReverb);
        bmp_cfg_db_set_bool(db,"modplug","FastInfo", mModProps.mFastinfo);
        bmp_cfg_db_set_bool(db,"modplug","UseFileName", mModProps.mUseFilename);
        bmp_cfg_db_set_bool(db,"modplug","GrabAmigaMOD", mModProps.mGrabAmigaMOD);
        bmp_cfg_db_set_bool(db,"modplug","PreAmp", mModProps.mPreamp);
        bmp_cfg_db_set_float(db,"modplug","PreAmpLevel", mModProps.mPreampLevel);
        bmp_cfg_db_set_int(db,"modplug", "Channels", mModProps.mChannels);
        bmp_cfg_db_set_int(db,"modplug", "Bits", mModProps.mBits);
        bmp_cfg_db_set_int(db,"modplug", "Frequency", mModProps.mFrequency);
        bmp_cfg_db_set_int(db,"modplug", "ResamplineMode", mModProps.mResamplingMode);
        bmp_cfg_db_set_int(db,"modplug", "ReverbDepth", mModProps.mReverbDepth);
        bmp_cfg_db_set_int(db,"modplug", "ReverbDelay", mModProps.mReverbDelay);
        bmp_cfg_db_set_int(db,"modplug", "BassAmount", mModProps.mBassAmount);
        bmp_cfg_db_set_int(db,"modplug", "BassRange", mModProps.mBassRange);
        bmp_cfg_db_set_int(db,"modplug", "SurroundDepth", mModProps.mSurroundDepth);
        bmp_cfg_db_set_int(db,"modplug", "SurroundDelay", mModProps.mSurroundDelay);
        bmp_cfg_db_set_int(db,"modplug", "LoopCount", mModProps.mLoopCount);

	bmp_cfg_db_close(db);
}

ModplugXMMS gModplugXMMS;
