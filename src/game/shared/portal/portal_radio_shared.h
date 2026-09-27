#ifndef PORTAL_RADIO_SHARED_H
#define PORTAL_RADIO_SHARED_H

enum RadioMode_t
{
	RADIO_DINOSAUR,					// Has signals and can be detected
	RADIO_NORMAL,					// No signals, no static
	RADIO_REPLACE_WITH_DINOSAUR,	// Changes to dinosaur mode if every player has completed the mapset
};

#endif // PORTAL_RADIO_SHARED_H