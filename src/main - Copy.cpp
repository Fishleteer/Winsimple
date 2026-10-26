#define WINSIMPLE_IMPL
#define WINSIMPLE_CONTROLS_IMPL
#define WINSIMPLE_AUDIO_IMPL
#define WINSIMPLE_CLIPBOARD_IMPL
#define WINSIMPLE_ANIMATION_IMPL
#define WINSIMPLE_VIDEO_IMPL

#include "winsimple.hpp"





int main()
{
	std::vector<ws::Window> windows;
	
	
	auto monitors = ws::Screen::getMonitors();
	for(auto& m : monitors)
	{
		windows.push_back(ws::Window());
		windows.back().create(m.getBounds().width,m.getBounds().height,"");
		windows.back().setBehindIcons();
	}
	ws::Font font;
	font.loadFromSystem("Arial");
	ws::Text text;
	text.setFont(font);
	
	
	bool running = true;
	while(running)
	{
		for(int a=0;a < windows.size();a++)
		{
			if(a >= windows.size() || a >= monitors.size())
				continue;
			ws::Window &w = windows[a]; 
			ws::Monitor &m = monitors[a];
			if(!w.isOpen())
			{
			//	windows.erase(windows.begin() + a);
			//	a-=2;
			//	continue;
			}
			w.clear(ws::Hue::red);
			
			text.setString(monitors[a].getDeviceName());
			text.setCharacterSize(100);
			text.setOrigin(text.getBounds().width/2,text.getBounds().height/2);
			text.setPosition(w.getSize().x/2,w.getSize().y/2);
			w.draw(text);
			
			w.display();
		}
		if(windows.empty())
			running = false;		
	}
	return 0;
}