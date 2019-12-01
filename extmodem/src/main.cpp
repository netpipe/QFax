/*
 * Sound card modem for Amateur Radio AX25.
 *
 * Copyright (C) Alejandro Santos LU4EXT, 2013, alejolp@gmail.com.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <iostream>

#include "audiosource_portaudio.h"
#include "audiosource_alsa.h"
#include "audiosource_null.h"
#include "audiosource_loopback.h"
#include "audiosource_wave.h"
#include "extmodem.h"
#include "extconfig.h"

#include "decoder_dtmf.h"
#include "decoder_af1200mm.h"
#include "decoder_af1200stj.h"
#include "decoder_af1200fsk.h"
#include "decoder_debug.h"

#include "encoder_af1200stj.h"

#include "frame.h"

#include <boost/smart_ptr.hpp>


using namespace extmodem;

int main(int argc, char **argv) {
	try {
		config* cfg = config::Instance();

		cfg->init(argc, argv);

		if (cfg->is_help()) {
			return 0;
		}

		int i;
		boost::shared_ptr<modem> em(new modem());
		boost::shared_ptr<audiosource> as;

		if (cfg->audio_backend() == "null") {
			as.reset(new audiosource_null(cfg->sample_rate()));
		} else if (cfg->audio_backend() == "loopback") {
			as.reset(new audiosource_loopback(cfg->sample_rate()));
		} else if (cfg->audio_backend() == "wave") {
			as.reset(new audiosource_wave(cfg->sample_rate()));
		} else if (cfg->audio_backend() == "portaudio") {
#ifdef PORTAUDIO_FOUND
			as.reset(new audiosource_portaudio(cfg->sample_rate()));
#else
			std::cerr << "ERROR: PORTAUDIO support not compiled" << std::endl;
			return 1;
#endif
		} else if (cfg->audio_backend() == "alsa") {
#if ALSA_FOUND
			as.reset(new audiosource_alsa(cfg->sample_rate()));
#else
			std::cerr << "ERROR: ALSA support not compiled" << std::endl;
			return 1;
#endif
		} else {
				std::cerr << "ERROR: Invalid audio backend" << std::endl;
				return 1;
		}

		if (cfg->list_devices()) {
			as->list_devices();
			return 0;
		}

		if (cfg->debug()) {
			std::cerr << "Starting extmodem ... " << std::endl;
		}

		em->set_audiosource(as);

		for (i = 0; i < cfg->in_chan_count(); ++i) {
			em->add_decoder(decoder_ptr(new decoder_dtmf(em.get())), i);
			em->add_decoder(decoder_ptr(new decoder_af1200mm(em.get())), i);
			em->add_decoder(decoder_ptr(new decoder_af1200stj(em.get())), i);
			em->add_decoder(decoder_ptr(new decoder_af1200fsk(em.get())), i);

			if (cfg->debugaudio()) {
				em->add_decoder(decoder_ptr(new decoder_debug(em.get())), i);
			}
		}

		em->set_encoder(encoder_ptr(new encoder_af1200stj()));

		if (cfg->debug()) {
			std::cerr << "Started!" << std::endl;
		}

		as->loop_async();

		em->start_and_run();
	} catch (std::exception& e) {
		std::cerr << "exception: " << e.what() << std::endl;
	}

	return 0;
}


