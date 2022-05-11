
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define ROWS 30
#define COLS 20

#define LEFT 1
#define RIGHT 2
#define UP 4
#define DOWN 8
#define DROP 16

int figures[][16] = {
  {1, 0, 0, 0,
   1, 0, 0, 0,
   1, 0, 0, 0,
   1, 0, 0, 0},

  {1, 1, 0, 0,
   1, 1, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0},

  {0, 1, 0, 0,
   1, 1, 0, 0,
   1, 0, 0, 0,
   0, 0, 0, 0},

  {1, 0, 0, 0,
   1, 1, 0, 0,
   0, 1, 0, 0,
   0, 0, 0, 0},

  {1, 1, 0, 0,
   1, 0, 0, 0,
   1, 0, 0, 0,
   0, 0, 0, 0},

  {1, 1, 0, 0,
   0, 1, 0, 0,
   0, 1, 0, 0,
   0, 0, 0, 0},

  {0, 1, 0, 0,
   1, 1, 1, 0,
   0, 0, 0, 0,
   0, 0, 0, 0}
};

int stop_running = 0, game_over = 0;
int field[COLS * ROWS];

int current_figure_initialized;
int current_figure[16], next_figure[16];
int current_x, current_y, current_color, next_color;

int speed = 30, speed_timer = 0;
int score = 0, lines = 0, level = 1;

int lines_to_remove[4];
int lines_to_remove_coint = 0;
int lines_to_remove_x = 0;

void get_next_figure() {
  int index = rand() % 7;
  current_color = next_color;
  next_color = 1 + rand() % 6;

  // Initiliaze current figure
  memcpy(current_figure, next_figure, 16 * sizeof(int));
  memcpy(next_figure, figures[index], 16 * sizeof(int));
  current_x = COLS / 2 - 1;
  current_y = 0;
  current_figure_initialized = 1;
}

int can_move(int new_x, int new_y) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (current_figure[x + y * 4]) {
        int tx = x + new_x, ty = y + new_y;
        if (tx < 0 || tx >= COLS || ty < 0 || ty >= ROWS || field[tx + ty * COLS])
          return 0;
      }
    }
  }
  return 1;
}

void drop() {
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      if (current_figure[x + y * 4])
        field[x + current_x + (y + current_y) * COLS] = current_color;

  lines_to_remove_coint = 0;
  lines_to_remove_x = 0;
  for (int y = current_y; y < current_y + 4 && y < ROWS; y++) {
    int r = 1;
    for (int x = 0; x < COLS; x++) {
      if (field[x + y * COLS] == 0) {
        r = 0;
        break;
      }
    }
    if (r)
      lines_to_remove[lines_to_remove_coint++] = y;
  }
  lines+= lines_to_remove_coint;
  score+= lines_to_remove_coint * 2;
  current_figure_initialized = 0;

  level = lines / 4;
  speed = 30 - level;
}

void rotate() {
  int rotated[16];
  int minx = 3, miny = 3;
  // Rotate
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int nx = 3 - y, ny = x;
      rotated[nx + ny * 4] = current_figure[x + y * 4];
      if (rotated[nx + ny * 4]) {
        if (minx > nx) minx = nx;
        if (miny > ny) miny = ny;
      }
    }
  }
  // Shift left/up
  for (int y = miny; y < 4; y++) {
    for (int x = minx; x < 4; x++) {
      int t = rotated[x + y * 4];
      rotated[x + y * 4] = 0;
      rotated[x - minx + (y - miny) * 4] = t;
    }
  }
  // Can place?
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (rotated[x + y * 4]) {
        int tx = x + current_x, ty = y + current_y;
        if (tx < 0 || tx >= COLS || ty < 0 || ty >= ROWS || field[tx + ty * COLS])
          return;
      }
    }
  }
  memcpy(current_figure, rotated, 16 * sizeof(int));
}

void render() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  int t = w.ws_col / 2 - COLS;
  char freespace[t + 1];
  memset(freespace, ' ', t);
  freespace[t] = 0;

  printf("\n\e[32m");
  printf("%s╭", freespace);
  for (int x = 0; x < COLS * 2; x++)
    printf("─");
  printf("╮\n");

  int shadow_min_x = COLS, shadow_max_x = 0;
  for (int y = 0; y < ROWS; y++) {
    printf("%s│", freespace);
    for (int x = 0; x < COLS; x++) {
      int color = field[x + y * COLS];
      if (color)
        printf("\e[3%im░░", color);
      else {
        int tx = x - current_x, ty = y - current_y;
        if (current_figure_initialized && tx >= 0 && tx <= 3 && ty >= 0 && ty <= 3 && current_figure[tx + ty * 4]) {
          // Render current figure
          printf("\e[3%im░░", current_color);
          if (shadow_min_x > x)
            shadow_min_x = x;
          if (shadow_max_x < x)
            shadow_max_x = x;
        } else
          printf("\e[3%im··", x >= shadow_min_x && x <= shadow_max_x ? 0 : 2);
      }
    }
    printf("\e[32m│  ");

    // Render next figure
    if (y == 0)
      printf("Next:");
    if (y >= 2 && y <= 2 + 3) {
      printf("\e[3%im", next_color);
      for (int x = 0; x < 4; x++) {
        if (next_figure[x + (y - 2) * 4])
          printf("░░");
        else
          printf("  ");
      }
      printf("\e[32m");
    }
    if (y == 8)
      printf("Score:");
    if (y == 10)
      printf("%i", score);
    if (y == 12)
      printf("Level:");
    if (y == 14)
      printf("%i", level);
    printf("\n");
  }

  printf("%s╰", freespace);
  for (int x = 0; x < COLS * 2; x++)
    printf("─");
  printf("╯\n\n");

  // Restore cursor position
  for (int y = 0; y < ROWS + 4; y++)
    printf("\e[A");
}

void clear() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  int t = w.ws_col / 2 + COLS + 2 + 5;
  char freespace[t + 1];
  memset(freespace, ' ', t);
  freespace[t] = 0;

  for (int y = 0; y < ROWS + 4; y++)
    printf("%s\n", freespace);

  // Restore cursor position
  for (int y = 0; y < ROWS + 4; y++)
    printf("\e[A");
}

void intHandler(int dummy) {
  stop_running = 1;
}

int is_key_pressed() {
 struct timeval tv;
 fd_set fds;
 tv.tv_sec = 0;
 tv.tv_usec = 0;

 FD_ZERO(&fds);
 FD_SET(STDIN_FILENO, &fds); 

 select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
 return FD_ISSET(STDIN_FILENO, &fds);
}

int main(int argc, char **argv) {
  printf("Keys: q - quit, h - right, j - down, k - rotate, l - left, space - drop\n");

  srand(time(NULL));

  // set the terminal to raw mode
  struct termios orig_term_attr, new_term_attr;
  tcgetattr(fileno(stdin), &orig_term_attr);
  memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
  new_term_attr.c_lflag &= ~(ICANON | ECHO);
  new_term_attr.c_cc[VTIME] = 0;
  new_term_attr.c_cc[VMIN] = 1;
  tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

  signal(SIGINT, intHandler);

  memset(field, 0, sizeof(int) * COLS * ROWS);
  get_next_figure();
  get_next_figure();

  // Hide cursor
  printf("\e[?25l");
  while (stop_running == 0 && game_over == 0) {
    render();
    // 60fps wait
    usleep(1000000 / 60);

    // Read keyboard
    int action = 0;
    while (is_key_pressed()) {
      int k = getchar();
      if (k == 0x1b || k == 'q')
        stop_running = 1;
      if (k == 'h')
        action|= LEFT;
      else if (k == 'l')
        action|= RIGHT;
      else if (k == 'k')
        action|= UP;
      else if (k == 'j')
        action|= DOWN;
      else if (k == ' ')
        action|= DROP;
    }

    if (current_figure_initialized) {
      // Process keyboard actions
      if (action & UP)
        rotate();
      if (action & LEFT && can_move(current_x - 1, current_y))
        current_x--;
      else if (action & RIGHT && can_move(current_x + 1, current_y))
        current_x++;
      if (action & DOWN && can_move(current_x, current_y + 1)) {
        current_y++;
        speed_timer+= speed / 2;
      }
      if (action & DROP) {
        while (can_move(current_x, current_y + 1))
          current_y++;
        speed_timer = speed;
      }

      speed_timer++;
      if (speed_timer > speed) {
        if (can_move(current_x, current_y + 1))
          current_y+= 1;
        else {
          drop();
          continue;
        }
        speed_timer = 0;
      }
    } else if (lines_to_remove_coint > 0) {
      for (int i = 0; i < lines_to_remove_coint; i++) {
        int y = lines_to_remove[i];
        if (y % 2)
          field[COLS - 1 - lines_to_remove_x + y * COLS] = 0;
        else
          field[lines_to_remove_x + y * COLS] = 0;
      }
      lines_to_remove_x++;
      if (lines_to_remove_x >= COLS) {
        // Move lines down
        for (int i = 0; i < lines_to_remove_coint; i++) {
          int y = lines_to_remove[i];
          memmove(field + COLS, field, y * COLS * sizeof(int));
          memset(field, 0, COLS * sizeof(int));
        }
        lines_to_remove_coint = 0;
      }
    } else {
      get_next_figure();
      if (!can_move(current_x, current_y))
        game_over = 1;
    }
  }
  clear();

  // Show cursor
  printf("\e[?25h\e[37m");

  if (game_over)
    printf("Game Over!\n");

  // Restore the original terminal attributes
  tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
  return 0;
}

