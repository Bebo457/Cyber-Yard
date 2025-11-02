#include "GameSettings.h"


namespace ScotlandYard {
	namespace Core {
		static GameSettings g_Settings;
		GameSettings& Settings() { return g_Settings; }
	}
} // namespaces
