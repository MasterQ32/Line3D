#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

#include <xnet/socket>
#include <xnet/dns>
#include <thread>

#include <set>

#include "mesh.hpp"

#define STR(_X) #_X
#define SSTR(_X) STR(_X)

#define CHECKED(_X) do { \
        if(int err = (_X); err != 0) { \
            fprintf(stderr, #_X " returned %d\n", err); \
            fflush(stderr); \
            exit(1); \
        } \
    } while(false)

typedef int OutCode;

const int INSIDE = 0; // 0000
const int LEFT = 1;   // 0001
const int RIGHT = 2;  // 0010
const int BOTTOM = 4; // 0100
const int TOP = 8;    // 1000

// Compute the bit code for a point (x, y) using the clip rectangle
// bounded diagonally by (xmin, ymin), and (xmax, ymax)

// ASSUME THAT xmax, xmin, ymax and ymin are global constants.

OutCode ComputeOutCode(float x, float y)
{
	OutCode code;

	code = INSIDE;          // initialised as being inside of [[clip window]]

	if (x < -1.0f)           // to the left of clip window
		code |= LEFT;
	else if (x > 1.0f)      // to the right of clip window
		code |= RIGHT;
	if (y < -1.0f)           // below the clip window
		code |= BOTTOM;
	else if (y > 1.0f)      // above the clip window
		code |= TOP;

	return code;
}

// Cohen–Sutherland clipping algorithm clips a line from
// P0 = (x0, y0) to P1 = (x1, y1) against a rectangle with
// diagonal from (xmin, ymin) to (xmax, ymax).
bool CohenSutherlandLineClipAndDraw(float & x0, float & y0, float & x1, float & y1)
{
	// compute outcodes for P0, P1, and whatever point lies outside the clip rectangle
	OutCode outcode0 = ComputeOutCode(x0, y0);
	OutCode outcode1 = ComputeOutCode(x1, y1);
	bool accept = false;

	while (true) {
		if (!(outcode0 | outcode1)) {
			// bitwise OR is 0: both points inside window; trivially accept and exit loop
			accept = true;
			break;
		} else if (outcode0 & outcode1) {
			// bitwise AND is not 0: both points share an outside zone (LEFT, RIGHT, TOP,
			// or BOTTOM), so both must be outside window; exit loop (accept is false)
			break;
		} else {
			// failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge
			float x, y;

			// At least one endpoint is outside the clip rectangle; pick it.
			OutCode outcodeOut = outcode0 ? outcode0 : outcode1;

			// Now find the intersection point;
			// use formulas:
			//   slope = (y1 - y0) / (x1 - x0)
			//   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
			//   y = y0 + slope * (xm - x0), where xm is xmin or xmax
			// No need to worry about divide-by-zero because, in each case, the
			// outcode bit being tested guarantees the denominator is non-zero
			if (outcodeOut & TOP) {           // point is above the clip window
				x = x0 + (x1 - x0) * (1.0f - y0) / (y1 - y0);
				y = 1.0f;
			} else if (outcodeOut & BOTTOM) { // point is below the clip window
				x = x0 + (x1 - x0) * (-1.0f - y0) / (y1 - y0);
				y = -1.0f;
			} else if (outcodeOut & RIGHT) {  // point is to the right of clip window
				y = y0 + (y1 - y0) * (1.0f - x0) / (x1 - x0);
				x = 1.0f;
			} else if (outcodeOut & LEFT) {   // point is to the left of clip window
				y = y0 + (y1 - y0) * (-1.0f - x0) / (x1 - x0);
				x = -1.0f;
			}

			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == outcode0) {
				x0 = x;
				y0 = y;
				outcode0 = ComputeOutCode(x0, y0);
			} else {
				x1 = x;
				y1 = y;
				outcode1 = ComputeOutCode(x1, y1);
			}
		}
	}
	if (accept) {
		// Following functions are left for implementation by user based on
		// their platform (OpenGL/graphics.h etc.)
		// DrawRectangle(xmin, ymin, xmax, ymax);
		// LineSegment(x0, y0, x1, y1);
        return true;
	}
    return false;
}

int main()
{
    CHECKED(SDL_Init(SDL_INIT_EVERYTHING));
    atexit(SDL_Quit);

    SDL_Window * window = SDL_CreateWindow(
        "Render Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_SHOWN
    );
    SDL_assert(window != nullptr);

    SDL_Renderer * ren = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
    );
    SDL_assert(ren != nullptr);

    auto const mesh= load_mesh("./models/terrain.obj");
    SDL_assert(mesh);

    glm::vec3 cameraPos(0, 0, 0);
    float cameraPan = 0.0f;
    float cameraTilt = 0.0f;

    xnet::socket sock(AF_INET, SOCK_STREAM);
    sock.connect(xnet::parse_ipv4("129.69.218.130", 3008));

    auto const quack = [&]() {
        sock.write("\x07", 1);
    };

    sock.write("Hello, Tek!\r\n", 13);

    auto const moveVector = [&](int x, int y)
    {
        unsigned char buf[4];
        buf[0] = 0x20 + (y / 32);
        buf[1] = 0x60 + (y % 32);
        buf[2] = 0x20 + (x / 32);
        buf[3] = 0x40 + (x % 32);
        sock.write(buf, sizeof buf);
    };

    auto const drawLine = [&](int x0, int y0, int x1, int y1)
    {
        auto const inBounds = [](int x, int y) {
            return (x >= 0) and (y >= 0) and (x < 1024) and (y < 780);
        };
        if(not inBounds(x0, y0) or not inBounds(x1, y1))
            return;

        sock.write("\x1D", 1);
        moveVector(x0, y0);
        moveVector(x1, y1);
    };

    int margin = 10;
    drawLine(margin, margin, 1023, margin);
    drawLine(1023 - margin, margin, 1023 - margin, 779 - margin);
    drawLine(1023 - margin, 779 - margin, margin, 779 - margin);
    drawLine(margin, 779 - margin, margin, margin);

    auto const mapTo = [](SDL_Rect const & rect, glm::vec2 pos) -> SDL_Point
    {
        return SDL_Point {
            int(rect.x + rect.w * (0.5f + 0.5f * pos.x)),
            int(rect.y + rect.h * (0.5f + 0.5f * pos.y)),
        };
    };

    auto const flipY = [](glm::vec2 p) {
        p.y = -p.y;
        return p;
    };

    SDL_Rect const screenRect = { 0, 0, 639, 479 };
    SDL_Rect const tekRect = { margin, margin, 1023 - margin, 779 - margin };

    quack();

    while(true)
    {
        auto const startOfFrame = SDL_GetTicks();

        bool renderOnTek = false;

        SDL_Event ev;
        while(SDL_PollEvent(&ev))
        {
            switch(ev.type)
            {
            case SDL_KEYDOWN:
                if(ev.key.keysym.sym == SDLK_ESCAPE)
                    goto _stop;
                if(ev.key.keysym.sym == SDLK_SPACE)
                    renderOnTek = true;
                break;
            case SDL_QUIT:
                goto _stop;
            }
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        auto const keys = SDL_GetKeyboardState(nullptr);

        cameraPan += 0.01f * ((keys[SDL_SCANCODE_LEFT]?1:0) - (keys[SDL_SCANCODE_RIGHT]?1:0));
        cameraTilt += 0.01f * ((keys[SDL_SCANCODE_PAGEUP]?1:0) - (keys[SDL_SCANCODE_PAGEDOWN]?1:0));

        auto const cameraRotPan = glm::angleAxis(cameraPan, glm::vec3(0, 1, 0));
        auto const cameraRotTilt = glm::angleAxis(cameraTilt, cameraRotPan * glm::vec3(1, 0, 0));

        auto const cameraRot = cameraRotTilt * cameraRotPan;

        auto const cameraLeft= cameraRot * glm::vec3(1,0,0);
        auto const cameraUp  = cameraRot * glm::vec3(0,1,0);
        auto const cameraForward = cameraRot * glm::vec3(0,0,-1);

        cameraPos += cameraLeft * float((keys[SDL_SCANCODE_D]?1:0) - (keys[SDL_SCANCODE_A]?1:0));
        cameraPos += cameraUp * float((keys[SDL_SCANCODE_HOME]?1:0) - (keys[SDL_SCANCODE_END]?1:0));
        cameraPos += cameraForward * float((keys[SDL_SCANCODE_W]|keys[SDL_SCANCODE_UP]?1:0) - (keys[SDL_SCANCODE_S]|keys[SDL_SCANCODE_DOWN]?1:0));

        auto const matPersp = glm::perspectiveFov<float>(
            glm::radians(60.0f),
            w, h,
            0.1f,
            1000.0f
        );

        auto const matView = glm::lookAt(
            cameraPos,
            cameraPos + cameraRot * glm::vec3(0, 0, -1),
            cameraRot * glm::vec3(0, 1, 0)
        );

        auto const matModel = glm::identity<glm::mat4>();

        auto const transform = matPersp * matView * matModel;

        SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x80, 0xFF);
        SDL_RenderClear(ren);

        struct Line
        {
            glm::vec2 a, b;

            Line() = default;
            Line(glm::vec2 _a, glm::vec2 _b) : a(_a), b(_b)
            {
                if((a.x < b.x) or ((a.x == b.x) and (a.y < b.y)))
                    std::swap(a, b);
            }

            bool operator<(Line const & o) const {
                return a.x < o.a.x or a.y < o.a.y;
            }
        };

        std::vector<Line> lines;
        lines.reserve(10000);

        for(auto const & face : mesh->faces)
        {
            Triangle transformed;
            std::transform(face.corners.begin(), face.corners.end(), transformed.corners.begin(), [&](Vertex vtx) {
                auto const v4d = transform * glm::vec4(vtx.position, 1.0);
                vtx.position = glm::vec3(v4d) / v4d.w;
                return vtx;
            });

            for(unsigned int i = 0; i < transformed.corners.size(); i++)
            {
                auto const & start = transformed.corners.at(i);
                auto const & end = transformed.corners.at((i + 1) % transformed.corners.size());

                if(start.position.z < 0.0f or end.position.z < 0.0f)
                    continue;

                if(start.position.z >= 1.0f or end.position.z >= 1.0f)
                    continue;

                Line l(start.position, end.position);

                if(not CohenSutherlandLineClipAndDraw(l.a.x, l.a.y, l.b.x, l.b.y))
                    continue;

                bool contain = false;
                for(size_t i = 0; i < lines.size(); i++) {
                    if(glm::distance(lines[0].a, l.a) > 0.001f)
                        continue;
                    if(glm::distance(lines[0].b, l.b) > 0.001f)
                        continue;
                    contain = true;
                    break;
                }

                if(not contain)
                    lines.emplace_back(l);
            }
        }

        // clip lines against polygons here!

        for(Line const & line : lines)
        {
            auto const scr0 = mapTo(screenRect, flipY(line.a));
            auto const scr1 = mapTo(screenRect, flipY(line.b));

            SDL_SetRenderDrawColor(ren, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderDrawLine(
                ren,
                scr0.x, scr0.y,
                scr1.x, scr1.y
            );

            if(renderOnTek) {
                auto const tek0 = mapTo(tekRect, line.a);
                auto const tek1 = mapTo(tekRect, line.b);

                drawLine(tek0.x, tek0.y, tek1.x, tek1.y);
            }
        }

        if(renderOnTek)
            quack();

        SDL_RenderPresent(ren);

        auto const endOfFrame = SDL_GetTicks();
        auto const frameTime = endOfFrame - startOfFrame;
        if(frameTime < 16)
            SDL_Delay(16 - frameTime);
    }
_stop:

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);

    return 0;
}
