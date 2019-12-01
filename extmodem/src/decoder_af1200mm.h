/*
 * decoder_af1200mm.h
 *
 *  Created on: 19/08/2013
 *      Author: alejo
 */
/*
 * Sound card modem for Amateur Radio AX25.
 *
 * Copyright (C) Alejandro Santos, 2013, alejolp@gmail.com.
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

#ifndef DECODER_AF1200MM_H_
#define DECODER_AF1200MM_H_

#include <vector>

#include "decoder.h"
#include "hdlc.h"


namespace extmodem {

/** "multimon" implementation of an AFSK1200 AX25 demodulator, by Thomas Sailer.
 *
 */
class decoder_af1200mm : public decoder {
public:
	decoder_af1200mm(modem* em);
	virtual ~decoder_af1200mm();

	virtual void input_callback(audiosource* a, const float* input, unsigned long frameCount);

private:
	void input_callback_real(audiosource* a, const float* input, unsigned long frameCount);

	struct l1_state_afsk12 {
		l1_state_afsk12() : dcd_shreg(0), sphase(0), lasts(0), subsamp(0) {}
		unsigned int dcd_shreg;
		unsigned int sphase;
		unsigned int lasts;
		unsigned int subsamp;
	} afsk12;

	hdlc hdlc_;
	std::vector<float> tmp_inbuffer_;

	unsigned int corrlen_;
	unsigned int sphaseinc_;
	std::vector<float> corr_mark_i;
	std::vector<float> corr_mark_q;
	std::vector<float> corr_space_i;
	std::vector<float> corr_space_q;
};

} /* namespace extmodem */
#endif /* DECODER_AF1200MM_H_ */
