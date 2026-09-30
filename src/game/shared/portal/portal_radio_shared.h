#ifndef PORTAL_RADIO_SHARED_H
#define PORTAL_RADIO_SHARED_H

enum RadioMode_t
{
	RADIO_NORMAL,							// No signals, no static
	RADIO_DINOSAUR_DEFAULT,					// Has signals and can be detected
	RADIO_DINOSAUR_REQUIRE_COMPLETION,		// Deletes itself if not everyone has completed the mapset
	RADIO_REPLACE_WITH_DINOSAUR,			// Changes to dinosaur mode if every player has completed the mapset
};

#endif // PORTAL_RADIO_SHARED_H