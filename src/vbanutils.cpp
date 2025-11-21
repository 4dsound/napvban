/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "vbanutils.h"

namespace nap
{

	bool utility::getVBANSampleRateFormatFromSampleRate(uint8_t& srFormat, int sampleRate)
	{
		for (int i = 0; i < VBAN_SR_MAXNUMBER; i++)
		{
			if (sampleRate == VBanSRList[i])
			{
				srFormat = i;
				return true;
			}
		}
		return false;
	}


	bool utility::getSampleRateFromVBANSampleRateFormat(int& sampleRate, uint8_t srFormat)
	{
		if (srFormat >= 0 && srFormat < VBAN_SR_MAXNUMBER)
		{
			sampleRate = VBanSRList[srFormat];
			return true;
		}
		return false;
	}

}