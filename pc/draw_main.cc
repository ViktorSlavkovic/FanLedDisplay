#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#include <boost/array.hpp>
#include <boost/asio.hpp>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::ostringstream;
using boost::asio::ip::udp;

enum DrawingMode { NONE, DRAW, CLEAR };

const int kScreenSize = 960;
const int kDrawOnMillis = 80;
const int kCircleAngularResoultion = 180;
const int kCircleNum = 60;
const int kRadiusStep = kScreenSize / 2 / kCircleNum;
const char *kUdpServerIp = "192.168.43.183";
const char *kUdpPort = "12345";

// 0 - clear
// 1 - to be cleared
// 2 - filled
// 3 - to be filled
int CircleParts[kCircleNum][kCircleAngularResoultion];

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Surface *surface;

boost::asio::io_service _io_service;
udp::resolver _resolver(_io_service);
udp::resolver::query _query(udp::v4(), kUdpServerIp, kUdpPort);
udp::endpoint _receiver_endpoint = *_resolver.resolve(_query);
udp::socket _socket(_io_service);

void InitGraphicsOrDie() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    cerr << " Failed to initialize SDL : " << SDL_GetError() << endl;
    exit(-1);
  }

  window =
      SDL_CreateWindow("Controller", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, kScreenSize, kScreenSize, 0);

  if (window == nullptr) {
    cerr << "Failed to create window : " << SDL_GetError() << endl;
    exit(-1);
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  if (renderer == nullptr) {
    cerr << "Failed to create renderer : " << SDL_GetError() << endl;
    exit(-1);
  }

  SDL_RenderSetLogicalSize(renderer, kScreenSize, kScreenSize);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
}

void Draw() {
  // Clear
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  // Pie Fields
  {
    int radius = kRadiusStep / 2;
    for (int circle = 0; circle < kCircleNum; circle++) {
      int angle_deg_step = 360 / kCircleAngularResoultion;
      int angle_deg = 0;
      for (int part = 0; part < kCircleAngularResoultion; part++) {
        for (int i = 0; i < 2; i++) {
          arcRGBA(renderer, kScreenSize / 2, kScreenSize / 2,
                  radius - kRadiusStep / 4 - i, angle_deg,
                  angle_deg + angle_deg_step,
                  CircleParts[circle][part] == 2 ? 255 : 0,
                  CircleParts[circle][part] == 3 ? 255 : 0,
                  CircleParts[circle][part] == 1 ? 255 : 0,
                  255);
        }
        angle_deg += angle_deg_step;
      }
      radius += kRadiusStep;
    }
  }

  // Bars
  {
    int angle_deg_step = 360 / kCircleAngularResoultion;
    int angle_deg = 0;
    for (int part = 0; part < kCircleAngularResoultion; part++) {
      pieRGBA(renderer, kScreenSize / 2, kScreenSize / 2,
              kRadiusStep / 2 + kRadiusStep * (kCircleNum - 1), angle_deg,
              angle_deg + angle_deg_step, 255, 255, 0, 40);
      angle_deg += angle_deg_step;
    }
  }

  // Circles
  {
    int radius = kRadiusStep / 2;
    for (int i = 0; i < kCircleNum; i++) {
      aacircleRGBA(renderer, kScreenSize / 2, kScreenSize / 2, radius, 255, 255,
                   0, 255);
      radius += kRadiusStep;
    }
  }

  // Show
  SDL_RenderPresent(renderer);
}

void QuitGraphics() {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void GetCiclePartFromXY(int x, int y, int *circle, int *part) {
  int radius = sqrt((x - kScreenSize / 2) * (x - kScreenSize / 2) +
                    (y - kScreenSize / 2) * (y - kScreenSize / 2));
  *circle = (radius <= kRadiusStep / 2)
            ? 0
            : (radius - kRadiusStep / 2) / kRadiusStep + 1;
  if (*circle > kCircleNum) {
    *circle = kCircleNum - 1;
  }
  double deg_angle =
      atan2((double)(kScreenSize / 2 - x), (double)(y - kScreenSize / 2)) /
      (2.0 * 3.14159265) * 360.0 + 90.0;
  if (deg_angle < 0) {
    deg_angle += 360.0;
  }
  *part = deg_angle / (360.0 / kCircleAngularResoultion);
  *part %= kCircleAngularResoultion;
}

void UpdateAndSend() {
  for (int circle = 0; circle < kCircleNum; circle++) {
    for (int part = 0; part < kCircleAngularResoultion; part++) {
      if (CircleParts[circle][part] % 2) {
        CircleParts[circle][part]--;
        cout << circle << ' ' << part << endl;
        boost::array<char, 2> send_buf;
        send_buf[0] = circle;
        send_buf[1] = part;
        _socket.send_to(boost::asio::buffer(send_buf), _receiver_endpoint);
      }
    }
  }
}

void SetCirclePart(int circle, int part, bool fill) {
  if (fill && CircleParts[circle][part] == 0) {
    CircleParts[circle][part] = 3;
  } else if (fill && CircleParts[circle][part] == 1) {
    CircleParts[circle][part] = 2;
  } else if (!fill && CircleParts[circle][part] == 2) {
    CircleParts[circle][part] = 1;
  } else if (!fill && CircleParts[circle][part] == 3) {
    CircleParts[circle][part] = 0;
  }
}

int main(int argc, char *args[]) {
  _socket.open(udp::v4());
  InitGraphicsOrDie();

  DrawingMode mode = NONE;
  SDL_Event event;
  uint32_t next_draw = SDL_GetTicks() + kDrawOnMillis;
  bool running = true;
  while (running) {
    if (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_KEYDOWN: {
          switch (event.key.keysym.sym) {
            case SDLK_c: {
              for (int circle = 0; circle < kCircleNum; circle++) {
                for (int part = 0; part < kCircleAngularResoultion; part++) {
                  SetCirclePart(circle, part, false);
                }
              }
              break;
            }
            case SDLK_u: {
              UpdateAndSend();
              break;
            }
          }
          break;
        }
        case SDL_MOUSEBUTTONUP: {
          mode = NONE;
          break;
        }
        case SDL_MOUSEBUTTONDOWN: {
          int circle, part;
          GetCiclePartFromXY(event.button.x, event.button.y, &circle, &part);
          switch (event.button.button) {
            case SDL_BUTTON_LEFT: {
              if (mode == NONE) {
                mode = DRAW;
              }
              SetCirclePart(circle, part, true);
              break;
            }
            case SDL_BUTTON_RIGHT: {
              if (mode == NONE) {
                mode = CLEAR;
              }
              SetCirclePart(circle, part, false);
              break;
            }
          }
          break;
        }
        case SDL_MOUSEMOTION: {
          int circle, part;
          GetCiclePartFromXY(event.button.x, event.button.y, &circle, &part);
          switch (mode) {
            case NONE:
              break;
            case DRAW: {
              SetCirclePart(circle, part, true);
              break;
            }
            case CLEAR: {
              SetCirclePart(circle, part, false);
              break;
            }
          }
          break;
        }
        case SDL_QUIT: {
          running = false;
          break;
        }
      }
    }
    if (SDL_TICKS_PASSED(SDL_GetTicks(), next_draw)) {
      next_draw = SDL_GetTicks() + kDrawOnMillis;
      Draw();
    }
  }

  QuitGraphics();
  return 0;
}
