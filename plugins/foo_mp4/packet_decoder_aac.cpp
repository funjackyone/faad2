#include "../SDK/foobar2000.h"
#include <faad.h>


class packet_decoder_aac : public packet_decoder
{
    faacDecHandle hDecoder;
    faacDecFrameInfo frameInfo;
	void cleanup()
	{
		if (hDecoder) {faacDecClose(hDecoder);hDecoder=0;}
	}
public:
	packet_decoder_aac()
	{
		hDecoder = 0;
	}

	~packet_decoder_aac()
	{
		cleanup();
	}
	
	virtual bool is_our_type(const char * name) {return !stricmp_utf8(name,"AAC");}
	
	virtual unsigned get_max_frame_dependency() {return 1;}
	
	virtual bool init(const void * data,unsigned bytes,file_info * info)
	{
		if (hDecoder) faacDecClose(hDecoder);
		if (data==0) return false;
		hDecoder = faacDecOpen();
		if (hDecoder == 0)
		{
			cleanup();
			console::error("Failed to open FAAD2 library.");
			return false;
		}

		{
			faacDecConfigurationPtr config;
            config = faacDecGetCurrentConfiguration(hDecoder);
            config->outputFormat = 
#if audio_sample_size == 64
				FAAD_FMT_DOUBLE
#else
				FAAD_FMT_FLOAT
#endif
				;
            faacDecSetConfiguration(hDecoder, config);
		}

		unsigned long t_samplerate;
		unsigned char t_channels;

		if(faacDecInit2(hDecoder, (unsigned char*)data, bytes,
						&t_samplerate,&t_channels) < 0)
		{
			faacDecClose(hDecoder);
			hDecoder = 0;
			return false;
		}

		{
			mp4AudioSpecificConfig mp4ASC;
			if (AudioSpecificConfig((unsigned char*)data, bytes, &mp4ASC) >= 0)
			{
				static const char *ot[6] = { "NULL", "MAIN AAC", "LC AAC", "SSR AAC", "LTP AAC", "HE AAC" };
				info->info_set("aac_profile",ot[(mp4ASC.objectTypeIndex > 5)?0:mp4ASC.objectTypeIndex]);
			}
		}



		return true;


	}
	
	virtual void reset_after_seek()
	{
		if (hDecoder)
		{
			faacDecPostSeekReset(hDecoder, -1);
		}
	}

	virtual bool decode(const void * buffer,unsigned bytes,audio_chunk * out)
	{
		const audio_sample * sample_buffer = (const audio_sample*)faacDecDecode(hDecoder, &frameInfo, (unsigned char*)buffer, bytes);

		if (frameInfo.error > 0)
		{
			cleanup();
			console::error(faacDecGetErrorMessage(frameInfo.error));
			return false;
		}

		if (frameInfo.channels<=0)
		{
			cleanup();
			console::error("Internal decoder error.");
			return false;
		}

		if (frameInfo.samples)
		{
			return out->set_data(sample_buffer,frameInfo.samples / frameInfo.channels,frameInfo.channels,frameInfo.samplerate);
		}
		else
		{
			out->reset();
			return true;
		}
	}
	virtual const char * get_name() {return "MPEG-4 AAC";}
};

static packet_decoder_factory<packet_decoder_aac> AHAHAHAHA;


bool is_valid_aac_decoder_config(const void * data,unsigned bytes)
{
	mp4AudioSpecificConfig mp4ASC;
	return AudioSpecificConfig((unsigned char*)data, bytes, &mp4ASC)>=0;
}