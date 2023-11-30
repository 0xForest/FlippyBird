#include <stdlib.h>
#include <flippy_bird_icons.h>
#include <furi.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/gui.h>
#include <input/input.h>
#include <dolphin/dolphin.h>
#include "phrases.h" // Edit contents for new phrases!

#define TAG "Flippy"
#define DEBUG false // Remember to change to false

#define FLAPPY_BIRD_HEIGHT 15
#define FLAPPY_BIRD_WIDTH 10

#define FLAPPY_PILAR_MAX 6
#define FLAPPY_PILAR_DIST 35

#define FLAPPY_GAB_HEIGHT 25
#define FLAPPY_GAB_WIDTH 10

#define FLAPPY_GRAVITY_JUMP -1.1
#define FLAPPY_GRAVITY_TICK 0.15

#define FLIPPER_LCD_WIDTH 128
#define FLIPPER_LCD_HEIGHT 64

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef enum { BirdState0 = 0, BirdState1, BirdState2, BirdStateMAX } BirdState;

const Icon* bird_states[BirdStateMAX] = {
    &I_flip_01,
    &I_flip_02,
    &I_flip_03,
};

typedef struct {
    int x;
    int y;
} POINT;

typedef struct {
    float gravity;
    POINT point;
} BIRD;

typedef struct {
    POINT point;
    int height;
    int visible;
    bool passed;
} PILAR;

typedef enum {
    GameStateLife,
    GameStateGameOver,
} State;

typedef struct {
    BIRD bird;
    int points;
    int pilars_count;
    PILAR pilars[FLAPPY_PILAR_MAX];
    bool debug;
    State state;
    const char* phrase;
} GameState;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

typedef enum {
    DirectionUp,
    DirectionRight,
    DirectionDown,
    DirectionLeft,
} Direction;

static void flappy_game_random_pilar(GameState* const game_state) {
    PILAR pilar;

    pilar.passed = false;
    pilar.visible = 1;
    pilar.height = random() % (FLIPPER_LCD_HEIGHT - FLAPPY_GAB_HEIGHT) + 1;
    pilar.point.y = 0;
    pilar.point.x = FLIPPER_LCD_WIDTH + FLAPPY_GAB_WIDTH + 1;

    game_state->pilars_count++;
    game_state->pilars[game_state->pilars_count % FLAPPY_PILAR_MAX] = pilar;
}

static void flappy_game_state_init(GameState* const game_state) {
    BIRD bird;
    bird.gravity = 0.0f;
    bird.point.x = 15;
    bird.point.y = 32;

    // Randomly select phrase
    int num_phrases = sizeof(encouraging_phrases) / sizeof(encouraging_phrases[0]);
    int random_index = rand() % num_phrases;

    game_state->phrase = encouraging_phrases[random_index];
    game_state->debug = DEBUG;
    game_state->bird = bird;
    game_state->pilars_count = 0;
    game_state->points = 0;
    game_state->state = GameStateLife;
    memset(game_state->pilars, 0, sizeof(game_state->pilars));

    flappy_game_random_pilar(game_state);
}

static void flappy_game_tick(GameState* const game_state) {
    if(game_state->state == GameStateLife) {
        if(!game_state->debug) {
            game_state->bird.gravity += FLAPPY_GRAVITY_TICK;
            game_state->bird.point.y += game_state->bird.gravity;
        }

        // Checking the location of the last respawned pilar.
        PILAR* pilar = &game_state->pilars[game_state->pilars_count % FLAPPY_PILAR_MAX];
        if(pilar->point.x == (FLIPPER_LCD_WIDTH - FLAPPY_PILAR_DIST))
            flappy_game_random_pilar(game_state);

        // Updating the position/status of the pilars (visiblity, position, game points)
        //        |  |      |  |  |
        //        |  |      |  |  |
        //        |__|      |  |__|
        //   _____X         |      X_____
        //  |     |         |      |     |   // [Pos + Width of pilar] >= [Bird Pos]
        //  |_____|         |      |_____|
        // X <---->         |     X <->
        // Bird Pos + Length of the  bird] >= [Pilar]
        for(int i = 0; i < FLAPPY_PILAR_MAX; i++) {
            PILAR* pilar = &game_state->pilars[i];
            if(pilar != NULL && pilar->visible && game_state->state == GameStateLife) {
                pilar->point.x--;
                if(game_state->bird.point.x >= pilar->point.x + FLAPPY_GAB_WIDTH &&
                   pilar->passed == false) {
                    pilar->passed = true;
                    game_state->points++;
                }
                if(pilar->point.x < -FLAPPY_GAB_WIDTH) pilar->visible = 0;

                if(game_state->bird.point.y <= 0 - FLAPPY_BIRD_WIDTH) {
                    game_state->bird.point.y = 64;
                }

                if(game_state->bird.point.y > 64 - FLAPPY_BIRD_WIDTH) {
                    game_state->bird.point.y = FLIPPER_LCD_HEIGHT - FLAPPY_BIRD_WIDTH;
                }

                // Bird inbetween pipes
                if((game_state->bird.point.x + FLAPPY_BIRD_HEIGHT >= pilar->point.x) &&
                   (game_state->bird.point.x <= pilar->point.x + FLAPPY_GAB_WIDTH)) {
                    // Bird below Bottom Pipe
                    if(game_state->bird.point.y + FLAPPY_BIRD_WIDTH - 2 >=
                       pilar->height + FLAPPY_GAB_HEIGHT) {
                        game_state->state = GameStateGameOver;
                        break;
                    }

                    // Bird above Upper Pipe
                    if(game_state->bird.point.y < pilar->height) {
                        game_state->state = GameStateGameOver;
                        break;
                    }
                }
            }
        }
    }
}

static void flappy_game_flap(GameState* const game_state) {
    game_state->bird.gravity = FLAPPY_GRAVITY_JUMP;
}

static void flappy_game_render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const GameState* game_state = ctx;

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    if(game_state->state == GameStateLife) {
        // Pilars
        for(int i = 0; i < FLAPPY_PILAR_MAX; i++) {
            const PILAR* pilar = &game_state->pilars[i];
            if(pilar != NULL && pilar->visible == 1) {
                canvas_draw_frame(
                    canvas, pilar->point.x, pilar->point.y, FLAPPY_GAB_WIDTH, pilar->height);

                canvas_draw_frame(
                    canvas, pilar->point.x + 1, pilar->point.y, FLAPPY_GAB_WIDTH, pilar->height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 2,
                    pilar->point.y,
                    FLAPPY_GAB_WIDTH - 1,
                    pilar->height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x,
                    pilar->point.y + pilar->height + FLAPPY_GAB_HEIGHT,
                    FLAPPY_GAB_WIDTH,
                    FLIPPER_LCD_HEIGHT - pilar->height - FLAPPY_GAB_HEIGHT);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 1,
                    pilar->point.y + pilar->height + FLAPPY_GAB_HEIGHT,
                    FLAPPY_GAB_WIDTH - 1,
                    FLIPPER_LCD_HEIGHT - pilar->height - FLAPPY_GAB_HEIGHT);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 2,
                    pilar->point.y + pilar->height + FLAPPY_GAB_HEIGHT,
                    FLAPPY_GAB_WIDTH - 1,
                    FLIPPER_LCD_HEIGHT - pilar->height - FLAPPY_GAB_HEIGHT);
            }
        }

        // Switch animation
        BirdState bird_state = BirdState1;
        if(game_state->bird.gravity < -0.5)
            bird_state = BirdState0;
        else if(game_state->bird.gravity > 0.5)
            bird_state = BirdState2;

        canvas_draw_icon(
            canvas, game_state->bird.point.x, game_state->bird.point.y, bird_states[bird_state]);

        canvas_set_font(canvas, FontSecondary);
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "Score: %u", game_state->points);
        canvas_draw_str_aligned(canvas, 100, 12, AlignCenter, AlignBottom, buffer);

        if(game_state->debug) {
            char coordinates[20];
            snprintf(coordinates, sizeof(coordinates), "Y: %u", game_state->bird.point.y);
            canvas_draw_str_aligned(canvas, 1, 12, AlignCenter, AlignBottom, coordinates);
        }
    }

    if(game_state->state == GameStateGameOver) {
        // Screen is 128x64 px
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 34, 20, 62, 24);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 34, 20, 62, 24);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 37, 31, "Game Over");

        canvas_set_font(canvas, FontSecondary);
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "Score: %u", game_state->points);
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignBottom, buffer);

        // Initiate encouragement protocol
        canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, game_state->phrase);
    }
}

static bool flappy_game_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    View* view = (View*)ctx;
    bool is_handled = false;

    if(input_event->type == InputTypePress) {
        switch(input_event->key) {
        case InputKeyUp:
            with_view_model(
                view,
                GameState * game_state,
                {
                    if(game_state->state == GameStateLife) {
                        flappy_game_flap(game_state);
                    }

                    is_handled = true;
                },
                true);
            break;
        case InputKeyOk:
            with_view_model(
                view,
                GameState * game_state,
                {
                    if(game_state->state == GameStateGameOver) {
                        flappy_game_state_init(game_state);
                    }

                    if(game_state->state == GameStateLife) {
                        flappy_game_flap(game_state);
                    }
                    is_handled = true;
                },
                true);
            break;
        default:
            is_handled = false;
            break;
        }
    }

    return is_handled;
}

static void flappy_game_update_timer_callback(void* context) {
    furi_assert(context);

    View* view = (View*)context;

    with_view_model(
        view, GameState * game_state, { flappy_game_tick(game_state); }, true);
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t skeleton_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

int32_t flippy_bird_game_app(void* p) {
    UNUSED(p);
    int32_t return_code = 0;

    //FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));

    // Set system callbacks
    View* view = view_alloc();

    view_allocate_model(
        view, ViewModelTypeLocking, sizeof(GameState)); // allocate model of size GameState

    with_view_model(
        view, GameState * game_state, { flappy_game_state_init(game_state); }, true);

    view_set_draw_callback(view, flappy_game_render_callback);
    view_set_context(view, view);
    view_set_input_callback(view, flappy_game_input_callback);
    view_set_previous_callback(view, skeleton_navigation_exit_callback);

    FuriTimer* timer =
        furi_timer_alloc(flappy_game_update_timer_callback, FuriTimerTypePeriodic, view);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 25);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    //gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    ViewDispatcher* view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(view_dispatcher);
    view_dispatcher_attach_to_gui(view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(view_dispatcher, 2, view);
    view_dispatcher_switch_to_view(view_dispatcher, 2);

    // Call dolphin deed on game start
    dolphin_deed(DolphinDeedPluginGameStart);

    view_dispatcher_run(view_dispatcher);

    furi_timer_free(timer);
    view_dispatcher_remove_view(view_dispatcher, 2);
    view_dispatcher_free(view_dispatcher);
    view_free_model(view);
    view_free(view);
    furi_record_close(RECORD_GUI);

    return return_code;
}