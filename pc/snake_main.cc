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
                  CircleParts[circle][part] == 1 ? 255 : 0, 255);
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

////////////////////////////////////////////////////////////////////////////////
//  CIRCULAR SNAKE
////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <list>
#include <ctime>
#include <algorithm>

using std::list;
using std::find;

struct SnakeElement {
  int circle;
  int part;
  bool operator== (const SnakeElement& rhs) {
    return circle == rhs.circle && part == rhs.part;
  }
};

const int kSnakeMinCircle = 30;

list<SnakeElement> Snake;
SnakeElement  Target;
int SnakeDirection;

const int DirectionDeltaCircle[4] = { -1,  0,  1,  0 };
const int DirectionDeltaPart[4]   = {  0,  1,  0, -1 };

SnakeElement GenerateRandomSnakeElement() {
  SnakeElement res;
  while (true) {
    res.circle = rand() % (kCircleNum - kSnakeMinCircle) + kSnakeMinCircle;
    res.part = rand() % kCircleAngularResoultion;
    if (find(Snake.begin(), Snake.end(), res) != Snake.end() || res == Target) {
      continue;
    }
    break;
  }
  return res;
}

void SnakeInit() {
  SnakeElement element = GenerateRandomSnakeElement();
  SetCirclePart(element.circle, element.part, true);
  Snake.push_back(element);
  element.part++;
  element.part %= kCircleAngularResoultion;
  SetCirclePart(element.circle, element.part, true);
  Snake.push_back(element);
  element.part++;
  element.part %= kCircleAngularResoultion;
  SetCirclePart(element.circle, element.part, true);
  Snake.push_back(element);
  Target = GenerateRandomSnakeElement();
  SetCirclePart(Target.circle, Target.part, true);
  SnakeDirection = 3;
  UpdateAndSend();
  Draw();
}

void SnakeSteer(bool left) {
  SnakeDirection += ((left) ? 1 : -1);
  if (SnakeDirection < 0) SnakeDirection = 3;
  if (SnakeDirection > 3) SnakeDirection = 0;
}

bool SnakeMove() {
  SnakeElement next_element = Snake.front();
  next_element.circle += DirectionDeltaCircle[SnakeDirection];
  if (next_element.circle < kSnakeMinCircle) {
    next_element.circle = kCircleNum - 1;
  }
  if (next_element.circle >= kCircleNum) {
    next_element.circle = kSnakeMinCircle;
  }
  next_element.part += DirectionDeltaPart[SnakeDirection];
  if (next_element.part < 0) next_element.part = kCircleAngularResoultion - 1;
  next_element.part %= kCircleAngularResoultion;
  // Game Over?
  if (find(Snake.begin(), Snake.end(), next_element) != Snake.end()) {
    for (int circle = 0; circle < kCircleNum; circle++) {
      for (int part = 0; part < kCircleAngularResoultion; part++) {
        SetCirclePart(circle, part, true);
      }
    }
    UpdateAndSend();
    Draw();
    return false;
  }
  // Eaten the target?
  if (next_element == Target) {
    Snake.push_front(next_element);
    //SetCirclePart(next_element.circle, next_element.part, true);
    Target = GenerateRandomSnakeElement();
    SetCirclePart(Target.circle, Target.part, true);
    UpdateAndSend();
    Draw();
  } else {
    SnakeElement last_element = Snake.back();
    SetCirclePart(last_element.circle, last_element.part, false);
    Snake.pop_back();
    SetCirclePart(next_element.circle, next_element.part, true);
    Snake.push_front(next_element);
    UpdateAndSend();
    Draw();
  }
  return true;
}

const int kMoveOnMillis = 50;

int main(int argc, char *args[]) {
  srand(time(0));

  _socket.open(udp::v4());
  InitGraphicsOrDie();

  SDL_Event event;
  uint32_t next_move = SDL_GetTicks() + kMoveOnMillis;
  bool running = true;
  bool pause = false;
  bool gameover = false;
  SnakeInit();
  while (running) {
    if (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN: {
        switch (event.key.keysym.sym) {
        case SDLK_a: {
          if (!pause && !gameover) {
            SnakeSteer(true);
          }
          break;
        }
        case SDLK_s: {
          if (!pause && !gameover) {
            SnakeSteer(false);
          }
          break;
        }
        case SDLK_p: {
          if (!gameover) {
            pause = !pause;
          }
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

    if (SDL_TICKS_PASSED(SDL_GetTicks(), next_move) && !pause) {
      next_move = SDL_GetTicks() + kMoveOnMillis;
      gameover = !SnakeMove();
    }
  }

  QuitGraphics();
  return 0;
}
