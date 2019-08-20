#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

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


    while(true)
    {
        auto const startOfFrame = SDL_GetTicks();

        SDL_Event ev;
        while(SDL_PollEvent(&ev))
        {
            switch(ev.type)
            {
            case SDL_KEYDOWN:
                if(ev.key.keysym.sym == SDLK_ESCAPE)
                    goto _stop;
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

        cameraPos += cameraLeft * float((keys[SDL_SCANCODE_A]?1:0) - (keys[SDL_SCANCODE_D]?1:0));
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

                int x0 = int(w * (0.5f + 0.5f * start.position.x));
                int y0 = int(h * (0.5f - 0.5f * start.position.y));
                int x1 = int(w * (0.5f + 0.5f * end.position.x));
                int y1 = int(h * (0.5f - 0.5f * end.position.y));

                SDL_SetRenderDrawColor(ren, 0xFF, 0xFF, 0xFF, 0xFF);
                SDL_RenderDrawLine(
                    ren,
                    x0, y0,
                    x1, y1
                );
            }
        }

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
