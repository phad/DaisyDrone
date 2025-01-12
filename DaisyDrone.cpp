#include <array>

#include "daisy_versio.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace daisy::seed;

DaisyVersio hw;

constexpr int NUM_TONES(5);
constexpr int NUM_POTS(NUM_TONES + 2);
constexpr int KNOBS[NUM_POTS] = {
	DaisyVersio::KNOB_0,
	DaisyVersio::KNOB_1,
	DaisyVersio::KNOB_2,
	DaisyVersio::KNOB_3,
	DaisyVersio::KNOB_4,
	DaisyVersio::KNOB_5,
	DaisyVersio::KNOB_6,
}; 

struct ToneSet
{
	float	m_base_frequency;
	char	m_note;
	bool	m_is_sharp;
};

constexpr int NUM_TONE_SETS(12);
ToneSet tones_sets[NUM_TONE_SETS] = {	{55.0f, 'A', false },
										{58.27f, 'A', true },
										{61.74f, 'B', false },
										{65.41f, 'C', false },
										{69.30f, 'C', true },
										{73.42f, 'D', false },
										{77.78f, 'D', true },
										{82.41f, 'E', false },
										{87.31f, 'F', false },
										{92.50f, 'F', true },
										{98.00f, 'G', false },
										{103.83, 'G', true } };
constexpr float DEFAULT_CENTS = 2.0f;

enum class WAVE_SUM_TYPE
{
	AVERAGE,
	SINE_WAVE_FOLD,
	TRIANGLE_WAVE_FOLD,
};

WAVE_SUM_TYPE sum_type = WAVE_SUM_TYPE::AVERAGE;

class DroneOscillator
{
	Oscillator		m_low_osc;
	Oscillator		m_base_osc;
	Oscillator		m_high_osc;
	Oscillator		m_pan_lfo;
	float			m_amplitude = 0.0f;

public:

	void initialise(float sample_rate)
	{
		auto init_osc =[sample_rate](Oscillator& osc)
		{
			osc.Init(sample_rate);
			osc.SetWaveform(osc.WAVE_SIN);
			osc.SetAmp(1.0f);
		};

		init_osc(m_low_osc);
		init_osc(m_base_osc);
		init_osc(m_high_osc);

		init_osc(m_pan_lfo);
		m_pan_lfo.SetFreq(0.2f);
		m_pan_lfo.SetAmp(0.5f);

		m_amplitude	= 0.0f;
	}

	void set_amplitude(float a)
	{
		m_amplitude = a;
	}

	void set_semitone( float base_frequency, int semitone, float cents )
	{
		auto semitone_to_frequency = [base_frequency](float semitone)->float
		{
			const float freq_mult	= powf( 2.0f, semitone / 12.0f );
			return base_frequency * freq_mult;
		};

		float cent_mult_low = 1.0f - (cents/100.0f);
		float cent_mult_high = 1.0f + (cents/100.0f);
		m_low_osc.SetFreq( semitone_to_frequency(semitone*cent_mult_low) );
		m_base_osc.SetFreq( semitone_to_frequency(semitone) );
		m_high_osc.SetFreq( semitone_to_frequency(semitone*cent_mult_high) );
		m_pan_lfo.SetFreq( semitone_to_frequency(semitone) / 256.0f );
	}

	void process(float* out_l, float* out_r)
	{
		const float avg_sin = (m_low_osc.Process() + m_base_osc.Process() + m_high_osc.Process() ) / 3;
		const float pan = m_pan_lfo.Process()+0.5f;

		*out_l = pan * avg_sin * m_amplitude;
		*out_r = (1.0f - pan) * avg_sin * m_amplitude;
	}
};

DroneOscillator oscillators[NUM_TONES];
float gain = 0.0f;

// https://www.desmos.com/calculator/ge2wvg2wgj
float triangular_wave_fold( float in )
{
	const float q_in = in * 0.25f;
	return 4 * (abs(q_in + 0.25f - roundf(q_in+0.25f))-0.25f);
}

float sin_wave_fold( float in )
{
	return sinf(in);
}

void audio_callback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		float osc_out_l = 0.0f, osc_out_r = 0.0f;
		float summed_out_l = 0.0f, summed_out_r = 0.0f;
		for( int o = 0; o < NUM_TONES; ++o )
		{
			oscillators[o].process(&osc_out_l, &osc_out_r);
			summed_out_l += osc_out_l;
			summed_out_r += osc_out_r;
		}

		switch(sum_type)
		{
			case WAVE_SUM_TYPE::AVERAGE:
			{
				summed_out_l /= NUM_TONES;
				summed_out_r /= NUM_TONES;
				break;
			}
			case WAVE_SUM_TYPE::SINE_WAVE_FOLD:
			{
				summed_out_l = sin_wave_fold(summed_out_l);
				summed_out_r = sin_wave_fold(summed_out_r);
				break;
			}
			case WAVE_SUM_TYPE::TRIANGLE_WAVE_FOLD:
			{
				summed_out_l = triangular_wave_fold(summed_out_l);
				summed_out_r = triangular_wave_fold(summed_out_r);
			}
		}

		summed_out_l *= gain;
		summed_out_r *= gain;
		
		out[0][i] = summed_out_l;
		out[1][i] = summed_out_r;
	}
}

void set_tones(float base_frequency, float cents, bool minor)
{
	constexpr int NUM_INTERVALS(4);
	// octave, 5th, octave, (min|maj)or 3rd
	const int intervals[NUM_INTERVALS] = { 12, 7, 5, minor?3:4 };
	int interval = 0;
	int semitone = 0;
	for( int t = 0; t < NUM_TONES; ++t )
	{
		oscillators[t].set_semitone(base_frequency, semitone, cents);

		semitone				+= intervals[interval];
		interval				= ( interval + 1 ) % NUM_INTERVALS;
	}
}

int main(void) {
    // Initialize Versio hardware.
    hw.Init();

    // Set up oscillators.
	const float sample_rate = hw.AudioSampleRate();
	for (DroneOscillator& osc : oscillators) {
		osc.initialise(sample_rate);
	}

	// Configure drone initial state.
	int current_tone_set = 0;
	const ToneSet& tone_set = tones_sets[current_tone_set];
	float current_cents = DEFAULT_CENTS;
	bool is_minor = true;
	set_tones(tone_set.m_base_frequency, current_cents, is_minor);

	// Start audio processing and ADC.
    hw.StartAudio(audio_callback);
    hw.StartAdc();

	bool prev_switch_pressed = false;

    while (true) {
        hw.ProcessAnalogControls(); // Normalize CV inputs
        hw.UpdateExample(); // Control the LED colors using the knobs and gate inputs
        hw.UpdateLeds();

		// Read knob value to set oscillators' amplitudes.
		for (int t = 0; t < NUM_TONES; ++t) {
			const float knob_val = hw.GetKnobValue(KNOBS[t]);
			oscillators[t].set_amplitude(knob_val);
		}

		// Read detune from knob 5.
		float prev_cents = current_cents;
		current_cents = DEFAULT_CENTS * hw.GetKnobValue(KNOBS[NUM_POTS - 2]);
		bool cents_changed = abs(current_cents - prev_cents) > 0.005f;

		// Read gain from knob 6.
		gain = hw.GetKnobValue(KNOBS[NUM_POTS - 1]);

		// Read sum type from switch 1.
		switch (hw.sw[DaisyVersio::SW_0].Read()) {
			case Switch3::POS_LEFT:	{
				sum_type = WAVE_SUM_TYPE::AVERAGE;
				break;
			}
			case Switch3::POS_CENTER: {
				sum_type = WAVE_SUM_TYPE::SINE_WAVE_FOLD;
				break;
			}
			case Switch3::POS_RIGHT: {
				sum_type = WAVE_SUM_TYPE::TRIANGLE_WAVE_FOLD;
				break;
			}
		}

		// Check direction of root change, if button is pressed.  
		const int dir(hw.sw[DaisyVersio::SW_1].Read());
		const int inc(dir == Switch3::POS_LEFT
						? -1
						: (dir == Switch3::POS_RIGHT ? 1 : 0));
		if (hw.SwitchPressed() && !prev_switch_pressed) {
			prev_switch_pressed = true;

			// If dir switch not centred, move up (down) a perfect 5th, 7 semitones.
			current_tone_set += 7*inc;
			if (current_tone_set > 0) {
				current_tone_set = current_tone_set % NUM_TONE_SETS;
			} else if (current_tone_set < 0) {
				current_tone_set = abs(current_tone_set) % NUM_TONE_SETS;
				current_tone_set = NUM_TONE_SETS - current_tone_set;
			}
			// Flip minor/major 3rds if direction switch is centred.
			if (dir == Switch3::POS_CENTER) {
				is_minor = !is_minor;
			}
		} else if (!hw.SwitchPressed()) {
			prev_switch_pressed = false;
		}

		if (prev_switch_pressed || cents_changed) {
			const ToneSet& tone_set = tones_sets[current_tone_set];
			set_tones(tone_set.m_base_frequency, current_cents, is_minor);
		}

        // Wait 1 ms.
        System::Delay(1);		
    }
}