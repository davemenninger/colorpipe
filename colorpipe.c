#include <SDL2/SDL.h>
#include <stdio.h>
#include <unistd.h>

/* Screen dimension constants */
const int COLUMN_WIDTH = 10;
const int FPS = 20;
const int SCREEN_HEIGHT = 64;
const int SCREEN_WIDTH = 640;
const int NUM_COLUMNS = SCREEN_WIDTH / COLUMN_WIDTH;

/* colorpipe will truncate your lines */
const int LINE_MAX = 512;

/* a color */
typedef struct rgb {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} rgb_t;

/* a ring buffer */
typedef struct ring {
  int head;
  int size;
  int tail;
  int full;
  rgb_t *buffer;
} ring_t;

typedef ring_t *ring_handle_t;

/* initialize the ring buffer */
ring_handle_t ring_init(rgb_t *buffer) {
  ring_handle_t handle = malloc(sizeof(ring_t));
  handle->buffer = buffer;
  handle->head = 0;
  handle->tail = 0;
  handle->size = NUM_COLUMNS;
  handle->full = 0;
  for (int i = 0; i < handle->size; i = i + 1) {
    rgb_t rgb = {'\0', '\0', '\0'};
    handle->buffer[i] = rgb;
  }

  return handle;
}

/* move head forward, with wraparound */
void advance_head(ring_handle_t handle) {
  if (handle->full == 1) {
    handle->tail = (handle->tail + 1) % handle->size;
  }

  handle->head = (handle->head + 1) % handle->size;
  handle->full = (handle->head == handle->tail) ? 1 : 0;
}

/* place a value in the buffer and advance the head */
void ring_put(ring_handle_t handle, rgb_t data) {
  handle->buffer[handle->head] = data;
  advance_head(handle);
}

/* debug the buffer to stderr */
void ring_print(ring_t *ring) {
  fprintf(stderr, "ring:\n");
  fprintf(stderr, "size: %i\n", ring->size);
  fprintf(stderr, "head: %i\n", ring->head);
  fprintf(stderr, "tail: %i\n", ring->tail);
  fprintf(stderr, "full: %i\n", ring->full);
  fprintf(stderr, "buffer: [ ");
  for (int i = 0; i < ring->size; i = i + 1) {
    fprintf(stderr, "{ ");
    fprintf(stderr, "%c, ", ring->buffer[i].red);
    fprintf(stderr, "%c, ", ring->buffer[i].green);
    fprintf(stderr, "%c, ", ring->buffer[i].blue);
    fprintf(stderr, " }");
  }
  fprintf(stderr, " ]\n");
}

/* copied from an example */
int error(char *msg, const char *err) {
  printf("Error %s: %s\n", msg, err);
  return 1;
}

/* compute a crude sum of some of the bytes */
unsigned char sum_bytes(char *line, int start, int end) {
  int sum = 0;
  for (int i = start; i < end; i = i + 1) {
    sum = sum + line[i];
    sum = sum % 256;
  }
  return sum;
}

int main(int argc, char *args[]) {
  (void)argc;
  (void)args;

  SDL_Window *window = NULL;
  SDL_Surface *surface = NULL;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return error("init", SDL_GetError());

  window = SDL_CreateWindow("colorpipe", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                            SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

  if (window == NULL)
    return error("window", SDL_GetError());

  Uint32 render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
  SDL_Renderer *rend = SDL_CreateRenderer(window, -1, render_flags);
  SDL_RenderClear(rend);

  SDL_bool quit = SDL_FALSE;
  SDL_Event event;

  SDL_Rect rect;
  rect.x = 0;
  rect.y = 0;
  rect.w = COLUMN_WIDTH;
  rect.h = SCREEN_HEIGHT;

  if (isatty(fileno(stdin)))
    fprintf(stderr, "stdin is a terminal\n");
  else
    fprintf(stderr, "stdin is a file or a pipe\n");

  rgb_t *buffer = malloc(NUM_COLUMNS * sizeof(rgb_t));
  ring_handle_t ring_handle = ring_init(buffer);
  rgb_t color = {'\0', '\0', '\0'};

  char line[LINE_MAX];

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        quit = SDL_TRUE;
        break;
      default:
        (void)0;
      }
    }

    /*
     * each line:
     *   put this rgb into the ring buffer
     *   draw the ring buffer onto the screen
     */
    if (fgets(line, sizeof(line), stdin) != NULL) {
      fputs(line, stdout);

      /* how much line did we get */
      int line_length = LINE_MAX;
      line_length = strcspn(line, "\n");
      int one_third = line_length / 3;
      int two_thirds = (line_length / 3) * 2;

      /* compute rgb from the bytes */
      color.red = sum_bytes(line, 0, one_third);
      color.green = sum_bytes(line, one_third, two_thirds);
      color.blue = sum_bytes(line, two_thirds, line_length);

      /* put it in the buffer */
      ring_put(ring_handle, color);
      /* ring_print(ring_handle); */

      /* draw */
      SDL_RenderClear(rend);

      /* draw every entry in the ring buffer */
      rgb_t column_color;
      int i = ring_handle->head;
      for (int count = 0; count < NUM_COLUMNS; count = count + 1) {
        column_color = ring_handle->buffer[i];

        rect.x = COLUMN_WIDTH * count;
        SDL_SetRenderDrawColor(rend, column_color.red, column_color.green,
                               column_color.blue, 255);
        SDL_RenderFillRect(rend, &rect);
        SDL_RenderDrawRect(rend, &rect);

        /* increment index with wraparound */
        i = (i - 1) % ring_handle->size;
        if (i < 0)
          i = ring_handle->size + i;
      }
    } else {
      /* fprintf(stderr, "end of file / nothing\n"); */
      quit = SDL_TRUE;
    }

    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderPresent(rend);
    SDL_Delay(1000 / FPS);
  }

  /* close */
  SDL_FreeSurface(surface);
  surface = NULL;
  SDL_DestroyWindow(window);
  window = NULL;
  SDL_Quit();

  return 0;
}
