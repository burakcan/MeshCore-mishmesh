// UIColor is defined per display driver in src/helpers/ui/*.cpp, none of which the
// native env compiles. Mono values (0/1) so they line up with DisplayDriver::DARK/LIGHT.
#include <helpers/ui/DisplayDriver.h>

ColorVal UIColor::window_bkg = 0;
ColorVal UIColor::title_bkg = 0;
ColorVal UIColor::title_txt = 1;
ColorVal UIColor::primary_txt = 1;
ColorVal UIColor::secondary_txt = 1;
ColorVal UIColor::warning_txt = 1;
ColorVal UIColor::popup_bkg = 0;
ColorVal UIColor::popup_txt = 1;
ColorVal UIColor::corp_blue = 1;
