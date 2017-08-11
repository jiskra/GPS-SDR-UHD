#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "uhdgps.h"
#include "gpssim.h"
#include "getch.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

sim_t *free_UHD_ptr;

void init_sim(sim_t *s)
{
	s->tx.dev = NULL;
	pthread_mutex_init(&(s->tx.lock), NULL);
	//s->tx.error = 0;

	pthread_mutex_init(&(s->gps.lock), NULL);
	//s->gps.error = 0;
	s->gps.ready = 0;
	pthread_cond_init(&(s->gps.initialization_done), NULL);

	s->status = 0;
	s->head = 0;
	s->tail = 0;
	s->sample_length = 0;

	pthread_cond_init(&(s->fifo_write_ready), NULL);
	pthread_cond_init(&(s->fifo_read_ready), NULL);

	s->time = 0.0;
}

size_t get_sample_length(sim_t *s)
{
	long length;

	length = s->head - s->tail;
	if (length < 0)
		length += FIFO_LENGTH;

	return((size_t)length);
}

size_t fifo_read(int16_t *buffer, size_t samples, sim_t *s)
{
	size_t length;
	size_t samples_remaining;
	int16_t *buffer_current = buffer;

	length = get_sample_length(s);

	if (length < samples)
		samples = length;

	length = samples; // return value

	samples_remaining = FIFO_LENGTH - s->tail;

	if (samples > samples_remaining) {
		memcpy(buffer_current, &(s->fifo[s->tail * 2]), samples_remaining * sizeof(int16_t) * 2);
		s->tail = 0;
		buffer_current += samples_remaining * 2;
		samples -= samples_remaining;
	}

	memcpy(buffer_current, &(s->fifo[s->tail * 2]), samples * sizeof(int16_t) * 2);
	s->tail += (long)samples;
	if (s->tail >= FIFO_LENGTH)
		s->tail -= FIFO_LENGTH;

	return(length);
}

bool is_finished_generation(sim_t *s)
{
	return s->finished;
}

int is_fifo_write_ready(sim_t *s)
{
	int status = 0;

	s->sample_length = get_sample_length(s);
	if (s->sample_length < NUM_IQ_SAMPLES)
		status = 1;

	return(status);
}

void *tx_task(void *arg)
{
	sim_t *s = (sim_t *)arg;
	size_t samples_populated;
	size_t num_samps_sent = 0;
	int16_t *buff;
	int firstpacket = 1;
	const void** buffs_ptr = NULL;

	buffs_ptr = (const void**)&buff;
	while (1) {
		int16_t *tx_buffer_current = s->tx.buffer;
		buff=s->tx.buffer;
		unsigned int buffer_samples_remaining = NUM_BUFFERS*s->tx.samps_per_buff;
		while (buffer_samples_remaining > 0) {

					pthread_mutex_lock(&(s->gps.lock));
					while (get_sample_length(s) == 0)
					{
						pthread_cond_wait(&(s->fifo_read_ready), &(s->gps.lock));
					}
		//			assert(get_sample_length(s) > 0);

					samples_populated = fifo_read(tx_buffer_current,
						buffer_samples_remaining,
						s);
					pthread_mutex_unlock(&(s->gps.lock));

					pthread_cond_signal(&(s->fifo_write_ready));
		#if 0
					if (is_fifo_write_ready(s)) {
						/*
						printf("\rTime = %4.1f", s->time);
						s->time += 0.1;
						fflush(stdout);
						*/
					}
					else if (is_finished_generation(s))
					{
						goto out;
					}
		#endif
					// Advance the buffer pointer.
					buffer_samples_remaining -= (unsigned int)samples_populated;
					tx_buffer_current += (2 * samples_populated);
				}
		// If there were no errors, transmit the data buffer.
		//bladerf_sync_tx(s->tx.dev, s->tx.buffer, SAMPLES_PER_BUFFER, NULL, TIMEOUT_MS);
		buffer_samples_remaining=NUM_BUFFERS*s->tx.samps_per_buff;
		//if (uhd_tx_metadata_make(&s->tx.md, false, 0.0, 0.0, false, false)){
		//			 	printf("Creating TX metadata error.\n");
		//			 	goto out;}//*/
		while(buffer_samples_remaining>0){
		if (buffer_samples_remaining<s->tx.samps_per_buff){
			 uhd_tx_streamer_send(s->tx.tx_streamer, buffs_ptr, buffer_samples_remaining, &s->tx.md, 0.1, &num_samps_sent);
		}else
			uhd_tx_streamer_send(s->tx.tx_streamer, buffs_ptr, s->tx.samps_per_buff, &s->tx.md, 0.1, &num_samps_sent);
		 buff+=2*sizeof(int16_t)*num_samps_sent;
		 buffer_samples_remaining-=num_samps_sent;
		 if (firstpacket) {
			 firstpacket=0;
			 if (uhd_tx_metadata_make(&s->tx.md, false, 0.0, 0.0, false, false)){
			 	printf("Creating TX metadata error.\n");
			 	goto out;}
		   }
		}
		//s->tx.
		if (is_fifo_write_ready(s)) {
			/*
			printf("\rTime = %4.1f", s->time);
			s->time += 0.1;
			fflush(stdout);
			*/
		}
		else if (is_finished_generation(s))
		{
			goto out;
		}

	}
out:
	return NULL;
}

int start_tx_task(sim_t *s)
{
	int status;

	status = pthread_create(&(s->tx.thread), NULL, tx_task, s);

	return(status);
}

int start_gps_task(sim_t *s)
{
	int status;

	status = pthread_create(&(s->gps.thread), NULL, gps_task, s);

	return(status);
}

void clear(int signo)
{
	printf("oops! stop!!!\n");
    if (free_UHD_ptr->tx.buffer != NULL)
    		free(free_UHD_ptr->tx.buffer);

    	if (free_UHD_ptr->fifo != NULL)
    		free(free_UHD_ptr->fifo);

    	printf("Closing device...\n");
    	//bladerf_close(s.tx.dev);
    	fprintf(stderr, "Cleaning up TX streamer.\n");
    	uhd_tx_streamer_free(&free_UHD_ptr->tx.tx_streamer);

    	fprintf(stderr, "Cleaning up TX metadata.\n");
    	uhd_tx_metadata_free(&free_UHD_ptr->tx.md);

    	/*if(return_code != EXIT_SUCCESS && free_UHD_ptr->tx.usrp != NULL){
    	   uhd_usrp_last_error(free_UHD_ptr->tx.usrp, error_string, 512);
    	   fprintf(stderr, "USRP reported the following error: %s\n", error_string);
    	        }*/
    	//printf(stderr, "Cleaning up USRP.\n");
    	//uhd_usrp_free(&free_UHD_ptr->tx.usrp);


    	//fprintf(stderr, (return_code ? "Failure\n" : "Success\n"));


    _exit(0);
}
void usage(void)
{
	printf("Usage: bladegps [options]\n"
		"Options:\n"
		"  -e <gps_nav>     RINEX navigation file for GPS ephemerides (required)\n"
		"  -u <user_motion> User motion file (dynamic mode)\n"
		"  -g <nmea_gga>    NMEA GGA stream (dynamic mode)\n"
		"  -l <location>    Lat,Lon,Hgt (static mode) e.g. 35.274,137.014,100\n"
		"  -t <date,time>   Scenario start time YYYY/MM/DD,hh:mm:ss\n"
		"  -T <date,time>   Overwrite TOC and TOE to scenario start time\n"
		"  -d <duration>    Duration [sec] (max: %.0f)\n"
		"  -U <USRP_ID>     Enable USRP, e.g. '-U <n>' 0 for Default\n"
		"  -a <tx_gain>     TX GAIN (default: %d)\n"
		"  -i               Interactive mode: North='%c', South='%c', East='%c', West='%c'\n"
		"  -I               Disable ionospheric delay for spacecraft scenario\n",
		((double)USER_MOTION_SIZE)/10.0, 
		TX_VGA1,
		NORTH_KEY, SOUTH_KEY, EAST_KEY, WEST_KEY);

	return;
}

int main(int argc, char *argv[])
{
	sim_t s;
	char devstr[10] = "";
	//int usrp_board=0;

	size_t channel = 0;
	double rate;
	double gain;
	double freq;
	double clock_bk;
	char error_string[512];
	int return_code = EXIT_SUCCESS;

	int result;
	double duration;
	datetime_t t0;

	free_UHD_ptr=&s;
	int txvga1 = TX_VGA1;

	if (argc<3)
	{
		usage();
		exit(1);
	}
	s.finished = false;

	s.opt.navfile[0] = 0;
	s.opt.umfile[0] = 0;
	s.opt.g0.week = -1;
	s.opt.g0.sec = 0.0;
	s.opt.iduration = USER_MOTION_SIZE;
	s.opt.verb = FALSE;
	s.opt.nmeaGGA = FALSE;
	s.opt.staticLocationMode = TRUE; // default user motion
	s.opt.llh[0] = 40.7850916 / R2D;
	s.opt.llh[1] = -73.968285 / R2D;
	s.opt.llh[2] = 100.0;
	s.opt.interactive = FALSE;
	s.opt.timeoverwrite = FALSE;
	s.opt.iono_enable = TRUE;

	while ((result=getopt(argc,argv,"e:u:g:l:T:t:d:x:a:U:iIv"))!=-1)
	{
		switch (result)
		{
		case 'e':
			strcpy(s.opt.navfile, optarg);
			break;
		case 'u':
			strcpy(s.opt.umfile, optarg);
			s.opt.nmeaGGA = FALSE;
			s.opt.staticLocationMode = FALSE;
			break;
		case 'g':
			strcpy(s.opt.umfile, optarg);
			s.opt.nmeaGGA = TRUE;
			s.opt.staticLocationMode = FALSE;
			break;
		case 'l':
			// Static geodetic coordinates input mode
			// Added by scateu@gmail.com
			s.opt.nmeaGGA = FALSE;
			s.opt.staticLocationMode = TRUE;
			sscanf(optarg,"%lf,%lf,%lf",&s.opt.llh[0],&s.opt.llh[1],&s.opt.llh[2]);
			s.opt.llh[0] /= R2D; // convert to RAD
			s.opt.llh[1] /= R2D; // convert to RAD
			break;
		case 'T':
			s.opt.timeoverwrite = TRUE;
			if (strncmp(optarg, "now", 3)==0)
			{
				time_t timer;
				struct tm *gmt;
				
				time(&timer);
				gmt = gmtime(&timer);

				t0.y = gmt->tm_year+1900;
				t0.m = gmt->tm_mon+1;
				t0.d = gmt->tm_mday;
				t0.hh = gmt->tm_hour;
				t0.mm = gmt->tm_min;
				t0.sec = (double)gmt->tm_sec;

				date2gps(&t0, &s.opt.g0);

			}
			break;
		case 't':
			sscanf(optarg, "%d/%d/%d,%d:%d:%lf", &t0.y, &t0.m, &t0.d, &t0.hh, &t0.mm, &t0.sec);
			if (t0.y<=1980 || t0.m<1 || t0.m>12 || t0.d<1 || t0.d>31 ||
				t0.hh<0 || t0.hh>23 || t0.mm<0 || t0.mm>59 || t0.sec<0.0 || t0.sec>=60.0)
			{
				printf("ERROR: Invalid date and time.\n");
				exit(1);
			}
			t0.sec = floor(t0.sec);
			date2gps(&t0, &s.opt.g0);
			break;
		case 'd':
			duration = atof(optarg);
			if (duration<0.0 || duration>((double)USER_MOTION_SIZE)/10.0)
			{
				printf("ERROR: Invalid duration.\n");
				exit(1);
			}
			s.opt.iduration = (int)(duration*10.0+0.5);
			break;
		case 'U':
			//usrp_board=atoi(optarg);
			s.tx.device_args=devstr;
			break;
		case 'v':
			s.opt.verb = TRUE;
			break;
		case 'a':
			txvga1 = atoi(optarg);
			break;
#if 0
			if (txvga1<BLADERF_TXVGA1_GAIN_MIN)
				txvga1 = BLADERF_TXVGA1_GAIN_MIN;
			else if (txvga1>BLADERF_TXVGA1_GAIN_MAX)
				txvga1 = BLADERF_TXVGA1_GAIN_MAX;
#endif
		case 'i':
			s.opt.interactive = TRUE;
			break;
		case 'I':
			s.opt.iono_enable = FALSE; // Disable ionospheric correction
			break;
		case ':':
		case '?':
			usage();
			exit(1);
		default:
			break;
		}
	}

	if (s.opt.navfile[0]==0)
	{
		printf("ERROR: GPS ephemeris file is not specified.\n");
		exit(1);
	}

	if (s.opt.umfile[0]==0 && !s.opt.staticLocationMode)
	{
		printf("ERROR: User motion file / NMEA GGA stream is not specified.\n");
		printf("You may use -l to specify the static location directly.\n");
		exit(1);
	}

	// Initialize simulator
	init_sim(&s);




	// Initializing device.
#if 0
	printf("Opening and initializing device...\n");

	s.status = bladerf_open(&s.tx.dev, devstr);
	if (s.status != 0) {
		fprintf(stderr, "Failed to open device: %s\n", bladerf_strerror(s.status));
		goto out;
	}

	if(usrp_board == 200) {
		printf("Initializing XB200 expansion board...\n");

		s.status = bladerf_expansion_attach(s.tx.dev, BLADERF_XB_200);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_filterbank(s.tx.dev, BLADERF_MODULE_TX, BLADERF_XB200_CUSTOM);
		if (s.status != 0) {
			fprintf(stderr, "Failed to set XB200 TX filterbank: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_path(s.tx.dev, BLADERF_MODULE_TX, BLADERF_XB200_BYPASS);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable TX bypass path on XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		//For sake of completeness set also RX path to a known good state.
		s.status = bladerf_xb200_set_filterbank(s.tx.dev, BLADERF_MODULE_RX, BLADERF_XB200_CUSTOM);
		if (s.status != 0) {
			fprintf(stderr, "Failed to set XB200 RX filterbank: %s\n", bladerf_strerror(s.status));
			goto out;
		}

		s.status = bladerf_xb200_set_path(s.tx.dev, BLADERF_MODULE_RX, BLADERF_XB200_BYPASS);
		if (s.status != 0) {
			fprintf(stderr, "Failed to enable RX bypass path on XB200: %s\n", bladerf_strerror(s.status));
			goto out;
		}
	}

	if(usrp_board == 300) {
		fprintf(stderr, "XB300 does not support transmitting on GPS frequency\n");
		goto out;
	}

	s.status = bladerf_set_frequency(s.tx.dev, BLADERF_MODULE_TX, TX_FREQUENCY);
	if (s.status != 0) {
		fprintf(stderr, "Faield to set TX frequency: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX frequency: %u Hz\n", TX_FREQUENCY);
	}

	s.status = bladerf_set_sample_rate(s.tx.dev, BLADERF_MODULE_TX, TX_SAMPLERATE, NULL);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX sample rate: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX sample rate: %u sps\n", TX_SAMPLERATE);
	}

	s.status = bladerf_set_bandwidth(s.tx.dev, BLADERF_MODULE_TX, TX_BANDWIDTH, NULL);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX bandwidth: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX bandwidth: %u Hz\n", TX_BANDWIDTH);
	}

	//s.status = bladerf_set_txvga1(s.tx.dev, TX_VGA1);
	s.status = bladerf_set_txvga1(s.tx.dev, txvga1);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX VGA1 gain: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		//printf("TX VGA1 gain: %d dB\n", TX_VGA1);
		printf("TX VGA1 gain: %d dB\n", txvga1);
	}

	s.status = bladerf_set_txvga2(s.tx.dev, TX_VGA2);
	if (s.status != 0) {
		fprintf(stderr, "Failed to set TX VGA2 gain: %s\n", bladerf_strerror(s.status));
		goto out;
	}
	else {
		printf("TX VGA2 gain: %d dB\n", TX_VGA2);
	}
#endif
#ifdef UHD_GPS
	if(uhd_set_thread_priority(uhd_default_thread_priority, true)){
		        fprintf(stderr, "Unable to set thread priority. Continuing anyway.\n");
		    }

		    if (s.tx.device_args == NULL){
		        s.tx.device_args = "";
		    }
		    // Create USRP
		    fprintf(stderr, "Creating USRP with args \"%s\"...\n", s.tx.device_args);
		        if (uhd_usrp_make(&s.tx.usrp, s.tx.device_args)){
		        	printf("Creating USRP error");
		        	goto out;}
		    //Change the USRP clock
		        if (uhd_usrp_set_master_clock_rate(s.tx.usrp,50e6,0)) return 1;
		    //See What the actual clock frequency
		        if (uhd_usrp_get_master_clock_rate(s.tx.usrp,0,&clock_bk)) return 1;
		        fprintf(stderr,"Master clock rate is %fMHz.\n",clock_bk/1e6);

		    // Create TX streamer
		        if (uhd_tx_streamer_make(&s.tx.tx_streamer)){
		        	printf("Creating Tx steamer error.\n");
		        	goto out;
		        }


		    // Create TX metadata
		    if (uhd_tx_metadata_make(&s.tx.md, false, 0.0, 0.0, false, false)){
		    	printf("Creating TX metadata error.\n");
		    	goto out;
		    }

		    // Create other necessary structs
		    uhd_tune_request_t tune_request = {
		        .target_freq = TX_FREQUENCY,
		        .rf_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO,
		        .dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO
		    };
		    uhd_tune_result_t tune_result;

		    uhd_stream_args_t stream_args = {
		        .cpu_format = "sc16",
		        .otw_format = "sc16",
		        .args = "",
		        .channel_list = &channel,
		        .n_channels = 1
		    };


		    // Set rate
		    fprintf(stderr, "Setting TX Rate: %d...\n", TX_SAMPLERATE);
		        if (uhd_usrp_set_tx_rate(s.tx.usrp, TX_SAMPLERATE, channel)){
		        	printf("Set rate error.\n");
		        	goto out;
		        }

		    // See what rate actually is
		    uhd_usrp_get_tx_rate(s.tx.usrp, channel, &rate);
		    fprintf(stderr, "Actual TX Rate: %f...\n\n", rate);

		    // Set gain
		    fprintf(stderr, "Setting TX Gain: %d db...\n", txvga1);
		    uhd_usrp_set_tx_gain(s.tx.usrp,txvga1 , 0, "");

		    // See what gain actually is
		    if (!uhd_usrp_get_tx_gain(s.tx.usrp, channel, "", &gain)){
		    fprintf(stderr, "Actual TX Gain: %f...\n", gain);}
		    else{
		    goto out;}

		    // Set frequency
		    fprintf(stderr, "Setting TX frequency: %f MHz...\n", TX_FREQUENCY/ 1e6);
		    if (uhd_usrp_set_tx_freq(s.tx.usrp, &tune_request, channel, &tune_result)){
		    	printf("Set frequency error.\n");
		    	goto out;
		    }

		    // See what frequency actually is
		     if (!uhd_usrp_get_tx_freq(s.tx.usrp, channel, &freq)){
		    fprintf(stderr, "Actual TX frequency: %f MHz...\n", freq / 1e6);}
		     else{
		    	 printf("Get actual frequency error.\n");
		    	 goto out;
		     }

		    // Set up streamer
		    stream_args.channel_list = &channel;
		        if (uhd_usrp_get_tx_stream(s.tx.usrp, &stream_args, s.tx.tx_streamer)){
		        	printf("Set up stream error.\n");
		        	goto out;
		        }

		    // Set up buffer
		    if (!uhd_tx_streamer_max_num_samps(s.tx.tx_streamer, &s.tx.samps_per_buff)){
		    	fprintf(stderr, "Buffer size in samples: %zu\n", s.tx.samps_per_buff);}
		    else {
		    	return 0;
		    }
#endif
			// Allocate TX buffer to hold each block of samples to transmit.
	s.tx.buffer = (int16_t *)malloc(NUM_BUFFERS*s.tx.samps_per_buff * sizeof(int16_t) * 2); // for 16-bit I and Q samples

	if (s.tx.buffer == NULL) {
		fprintf(stderr, "Failed to allocate TX buffer.\n");
		goto out;
		}

	// Allocate FIFOs to hold 0.1 seconds of I/Q samples each.
	s.fifo = (int16_t *)malloc(FIFO_LENGTH * sizeof(int16_t) * 2); // for 16-bit I and Q samples

	if (s.fifo == NULL) {
		fprintf(stderr, "Failed to allocate I/Q sample buffer.\n");
		goto out;
		}
	// Start GPS task.goto out;
	s.status = start_gps_task(&s);
	if (s.status < 0) {
		fprintf(stderr, "Failed to start GPS task.\n");
		goto out;
	}
	else
		printf("Creating GPS task...\n");

	// Wait until GPS task is initialized
	pthread_mutex_lock(&(s.tx.lock));
	while (!s.gps.ready)
		pthread_cond_wait(&(s.gps.initialization_done), &(s.tx.lock));
	pthread_mutex_unlock(&(s.tx.lock));

	// Fillfull the FIFO.
	if (is_fifo_write_ready(&s))
		pthread_cond_signal(&(s.fifo_write_ready));
#if 0

	// Configure the TX module for use with the synchronous interface.
	s.status = bladerf_sync_config(s.tx.dev,
			BLADERF_MODULE_TX,
			BLADERF_FORMAT_SC16_Q11,
			NUM_BUFFERS,
			SAMPLES_PER_BUFFER,
			NUM_TRANSFERS,
			TIMEOUT_MS);

	if (s.status != 0) {
		fprintf(stderr, "Failed to configure TX sync interface: %s\n", bladerf_strerror(s.status));
		goto out;
	}

	// We must always enable the modules *after* calling bladerf_sync_config().
	s.status = bladerf_enable_module(s.tx.dev, BLADERF_MODULE_TX, true);
	if (s.status != 0) {
		fprintf(stderr, "Failed to enable TX module: %s\n", bladerf_strerror(s.status));
		goto out;
	}
#endif
	// Start TX task
	s.status = start_tx_task(&s);
	if (s.status < 0) {
		fprintf(stderr, "Failed to start TX task.\n");
		goto out;
	}
	else
		printf("Creating TX task...\n");

	// Running...
	printf("Running...\n");
	printf("Press 'Ctrl+C' to abort.\n");

	signal(SIGINT, clear);

	// Wainting for TX task to complete.
	pthread_join(s.tx.thread, NULL);
	printf("\nDone!\n");
#if 0
	// Disable TX module and shut down underlying TX stream.
	s.status = bladerf_enable_module(s.tx.dev, BLADERF_MODULE_TX, false);
	if (s.status != 0)
		fprintf(stderr, "Failed to disable TX module: %s\n", bladerf_strerror(s.status));
#endif

out:
	// Free up resources
	if (s.tx.buffer != NULL)
		free(s.tx.buffer);

	if (s.fifo != NULL)
		free(s.fifo);

	printf("Closing device...\n");
	//bladerf_close(s.tx.dev);
	fprintf(stderr, "Cleaning up TX streamer.\n");
	uhd_tx_streamer_free(&s.tx.tx_streamer);

	fprintf(stderr, "Cleaning up TX metadata.\n");
	uhd_tx_metadata_free(&s.tx.md);

	if(return_code != EXIT_SUCCESS && s.tx.usrp != NULL){
	   uhd_usrp_last_error(s.tx.usrp, error_string, 512);
	   fprintf(stderr, "USRP reported the following error: %s\n", error_string);
	        }
	uhd_usrp_free(&s.tx.usrp);


	fprintf(stderr, (return_code ? "Failure\n" : "Success\n"));

	return(0);
}
