#include <iostream>
#include <time.h>
#include <string>

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>

ALLEGRO_FONT* font;
ALLEGRO_DISPLAY* display;
ALLEGRO_EVENT_QUEUE* queue;
ALLEGRO_TIMER* timer;

void initialize();
void terminate();
void frame_update();

int main(int argc, char const* argv[])
{
	initialize();


	bool running = true;
	while(running) {
		ALLEGRO_EVENT event;
		al_wait_for_event(queue, &event);
		if(event.type == ALLEGRO_EVENT_KEY_UP || event.type == ALLEGRO_EVENT_DISPLAY_CLOSE)
			running = false;
		else if(event.type == ALLEGRO_EVENT_TIMER) {
			frame_update();
		}

	}


	terminate();

	return 0;
}

void initialize() {
	if(!al_init()) fprintf(stderr, "Failed to initialize allegro.\n");
	if(!al_init_font_addon()) fprintf(stderr, "Failed to initialize font addon.\n");
	if(!al_init_ttf_addon()) fprintf(stderr, "Failed to initialize ttf addon.\n");
	if(!al_install_keyboard()) fprintf(stderr, "Failed to install keyboard.\n");

	font = al_load_ttf_font("fonts/Roboto/Roboto-Medium.ttf", 20, 0);
	display = al_create_display(640, 480);
	queue = al_create_event_queue();
	timer = al_create_timer(1.0 / 60);

	if(!font) fprintf(stderr, "Failed load font.\n");
	if(!display) fprintf(stderr, "Failed initialize display.\n");
	if(!queue) fprintf(stderr, "Failed create queue.\n");
	if(!timer) fprintf(stderr, "Failed to create timer.\n");

	al_register_event_source(queue, al_get_keyboard_event_source());
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_start_timer(timer);
}

void terminate()
{
	al_uninstall_keyboard();
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	al_destroy_font(font);
}

void frame_update() {
	al_clear_to_color(al_map_rgb(0x20, 0x15, 0x15));
	al_draw_text(font, al_map_rgb(0xff, 0xff, 0xff), 0, 0, 0, "This is some text");
	al_flip_display();
}


