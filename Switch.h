#ifndef __SWITCH_H__
#define __SWITCH_H__

class Switch {
public:
	Switch(unsigned millis): _millis(millis) {}

	operator bool() {
		bool on = _on;
		if (on) {
			_on = false;
			_reset = millis();
		}
		return on;
	}

	void on() {
		unsigned now = millis();
		if (now > _reset + _millis) {
			_on = true;
		}
	}

private:
	unsigned _millis, _reset;
	bool _on;
};

#endif
