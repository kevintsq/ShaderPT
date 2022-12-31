#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "glad_wgl.h"

// try dedicated GPU
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllexport) int NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#ifdef __cplusplus
}
#endif

typedef struct vec3 {
  float x;
  float y;
  float z;
} vec3;

enum MaterialType { DIFFUSE, SPECULAR, REFRACTIVE };

typedef struct Sphere {
    vec3 center;
    float radius;
    vec3 emission;
    float padding;  // add dummy padding to make the struct size a multiple of 16
    vec3 color;
    float material;
} Sphere;

Sphere spheres[] = {  // center.xyz, radius  |  emission.xyz, 0  |  color.rgb, material
        {{-1e5f - 2.6f, 0, 0}, 1e5f, {0, 0, 0}, 0, {0, 91.f/255, 172.f/255}, DIFFUSE}, // Left
        {{1e5f + 2.6f, 0, 0}, 1e5f, {0, 0, 0}, 0, {139.f/255, 0, 18.f/255}, DIFFUSE}, // Right
        {{0, 1e5f + 2, 0}, 1e5f, {0, 0, 0}, 0, {.75f, .75f, .75f}, DIFFUSE}, // Top
        {{0, -1e5f - 2, 0}, 1e5f, {0, 0, 0}, 0, {.75f, .75f, .75f}, DIFFUSE}, // Bottom
        {{0, 0, -1e5f - 2.8f}, 1e5f, {0, 0, 0}, 0, {.75f, .75f, .75f}, DIFFUSE}, // Back
        {{0, 0, 1e5f + 7.9f}, 1e5f, {0, 0, 0}, 0, {.75f, .75f, .75f}, DIFFUSE}, // Front
        {{-1.3f, -1.2f, -1.3f}, 0.8f, {0, 0, 0}, 0, {.999f, .999f, .999f}, SPECULAR}, // BALL 1
        {{1.3f, -1.2f, -0.2f}, 0.8f, {0, 0, 0}, 0, {.999f, .999f, .999f}, REFRACTIVE}, // BALL 2
        {{0, 11.96f, 0}, 10, {16, 16, 16}, 0, {0, 0, 0}, DIFFUSE}, // Light
};

void PrintLastError(const char *name) {
    LPSTR msg;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &msg, 0, NULL);
    MessageBox(NULL, msg, name, MB_OK | MB_ICONERROR);
    LocalFree(msg);
    exit(1);
}

void CheckProgramStatus(GLuint program) {
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        GLchar *log = (GLchar *) malloc(length);
        glGetProgramInfoLog(program, length, NULL, log);
        MessageBox(NULL, log, "Program Linking Error", MB_OK | MB_ICONERROR);
        free(log);
        exit(EXIT_FAILURE);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE: {
            // According to documentation low-order word of lParam gives us screen width pixel size,
            // and high-order word of lParam gives height pixel size.
            // https://docs.microsoft.com/en-us/windows/desktop/winmsg/wm-size
            GLint width = LOWORD(lParam);
            GLint height = HIWORD(lParam);
            glViewport(0, 0, width, height);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// Debug callback for OpenGL 4.3 feature glDebugOutput
// https://www.khronos.org/opengl/wiki/OpenGL_Error#Catching_errors_.28the_easy_way.29
void GLAPIENTRY MessageCallback(GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar *message,
                                const void *userParam) {
    if (type == GL_DEBUG_TYPE_ERROR) {
        MessageBox(NULL, message, "OpenGL Error", MB_OK | MB_ICONERROR);
        exit(1);
    }
}

GLuint CompileGLShaderFile(const char *filename, GLenum shaderType) {
    HANDLE shaderFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (shaderFile == INVALID_HANDLE_VALUE) {
        PrintLastError(filename);
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(shaderFile, &size)) {
        PrintLastError(filename);
    }

    char *shaderSource = malloc(size.QuadPart + 1);
    DWORD readSize;
    if (!ReadFile(shaderFile, (LPVOID) shaderSource, size.QuadPart, &readSize, NULL) || readSize != size.QuadPart) {
        PrintLastError(filename);
    }
    // Put 0 char on end of the source buffer for indicating this file ended.
    shaderSource[size.QuadPart] = '\0';
    CloseHandle(shaderFile);

    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const GLchar *const *) &shaderSource, 0);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = (GLchar *) malloc(length);
        glGetShaderInfoLog(shader, length, &length, info);
        MessageBox(NULL, info, "Shader Error", MB_OK | MB_ICONERROR);
        exit(EXIT_FAILURE);
    }

    free(shaderSource);
    return shader;
}

int main(int argc, char *argv[]) {
    int HEIGHT = argc > 1 ? atoi(argv[1]) : 720;
    int WIDTH = HEIGHT * 3 / 2;    // image resolution

    // Window creation
    WNDCLASS windowClass = {0};
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = "OpenGL Ray Tracing";

    RegisterClass(&windowClass);

    HWND windowHandle = CreateWindow(windowClass.lpszClassName,
                                     windowClass.lpszClassName,
                                     WS_OVERLAPPEDWINDOW,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     WIDTH,
                                     HEIGHT,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);

    HDC deviceContext = GetDC(windowHandle);
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

    int pixelFormat = ChoosePixelFormat(deviceContext, &pfd);
    SetPixelFormat(deviceContext, pixelFormat, &pfd);

    // Create OpenGL context
    HGLRC wglContext = wglCreateContext(deviceContext);
    // To be able to use wgl extensions, we have to make current a WGL context.
    // Otherwise, we can't get function pointers to WGL extensions.
    wglMakeCurrent(deviceContext, wglContext);

    // Load WGL extensions.
    gladLoadWGL(deviceContext);

    // We use OpenGL version 4.6 core profile and debug context for reasonable error messages.
    int glAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 6,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0,
    };

    // Create new WGL context with attributes, using WGL extensions.
    // And make current the new one and delete the old one.
    HGLRC newContext = wglCreateContextAttribsARB(deviceContext, 0, glAttribs);
    wglMakeCurrent(deviceContext, newContext);
    wglDeleteContext(wglContext);

    gladLoadGL();

    ShowWindow(windowHandle, SW_NORMAL);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);
    // Print Info
    printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
    printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
    printf("GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Compute program
    GLuint computeShader = CompileGLShaderFile("compute.glsl", GL_COMPUTE_SHADER);

    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);
    CheckProgramStatus(computeProgram);

    // Draw program
    GLuint vertexShader = CompileGLShaderFile("vertex.glsl", GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileGLShaderFile("fragment.glsl", GL_FRAGMENT_SHADER);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    CheckProgramStatus(shaderProgram);

    const float buffer[] = {-1.0f, -1.0f, 0.0f, 0.0f,
                            -1.0f, 1.0f, 0.0f, 1.0f,
                            1.0f, 1.0f, 1.0f, 1.0f,
                            1.0f, -1.0f, 1.0f, 0.0f};

    const GLuint elementBuffer[] = {0, 1, 2,
                                    3, 0, 2};

    GLuint vbo;
    glGenBuffers(1, &vbo);

    GLuint ebo;
    glGenBuffers(1, &ebo);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(buffer), buffer, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elementBuffer), elementBuffer, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH,
                 HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Compute shader initialization
    GLuint ssbObject;
    glGenBuffers(1, &ssbObject);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbObject);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, sizeof(spheres), spheres, 0);

    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    int frameIndex = 0;
    double totalTime = 0;
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            GLuint timeQuery;
            glGenQueries(1, &timeQuery);
            glBeginQuery(GL_TIME_ELAPSED, timeQuery);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbObject);

            glUseProgram(computeProgram);
            // We treat our main texture as a double buffer.
            // It is maybe not safe that using the same object for double buffer. 
            // But in the shader, we first read pixel and calculate color and write new color.
            // So there is no conflict on pixels. So we are okay for now!
            glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
            glBindImageTexture(1, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            glUniform2ui(glGetUniformLocation(computeProgram, "imgSize"), WIDTH, HEIGHT);
            glUniform1ui(glGetUniformLocation(computeProgram, "frameIndex"), frameIndex);
            glDispatchCompute((WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            // Unbind storage buffers
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

            glUseProgram(shaderProgram);
            glActiveTexture(GL_TEXTURE0 + 2);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(glGetUniformLocation(shaderProgram, "image"), 2);
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            frameIndex++;

            glEndQuery(GL_TIME_ELAPSED);
            GLuint available = 0;
            while (!available) {
                glGetQueryObjectuiv(timeQuery, GL_QUERY_RESULT_AVAILABLE, &available);
            }

            GLuint64 elapsedTimeNanos = 0;
            glGetQueryObjectui64v(timeQuery, GL_QUERY_RESULT, &elapsedTimeNanos);
            char cad[64];
            totalTime += (double) elapsedTimeNanos / 1000000000;
            sprintf(cad, "Frame: %d, FPS: %.2f", frameIndex, frameIndex / totalTime);
            SetWindowText(windowHandle, cad);

            SwapBuffers(deviceContext);
        }
    }
    return 0;
}
