#include <windows.h>
#include <cstdio>
#include <cassert>
//for GLEW
#include "../GLEW/include/GL/glew.h"
#include <GL/GL.h>
//for loading png
#include "../LodePNG/lodepng.h"
//for math
#include "../glm/glm.hpp"
#include "../glm/gtc/matrix_transform.hpp" // translate, rotate, scale, perspective
#include "../glm/ext.hpp" // perspective, translate, rotate
#include "../glm/gtc/type_ptr.hpp" // value_ptr

#pragma comment (lib, "../../GLEW/glew32.lib")
#pragma comment (lib, "opengl32.lib")

//Global
HDC hDC;
HGLRC openGLRC;
RECT clientRect;

//OpenGL
GLuint vertexBuf = 0;
GLuint indexBuf = 0;
GLuint texture = 0;
GLuint vShader = 0;
GLuint pShader = 0;
GLuint normalProgram = 0;
GLint attributePos = 0;
GLint attributeTexCoord = 0;
GLint uniformProjMtx = 0;

GLuint flatColorProgram = 0;

GLuint blurProgram = 0;
GLuint framebuffer = 0;
GLuint textureColorBuffer = 0;
GLuint renderBuffer = 0;
GLuint quadVAO, quadVBO;
GLint attributePosQuad = 0;
GLint attributeTexCoordQuad = 0;

GLenum err = GL_NO_ERROR;

//image
unsigned char* image;
unsigned width, height;

//misc
int clientWidth;
int clientHeight;
BOOL CreateOpenGLRenderContext(HWND hWnd);
BOOL InitGLEW();
BOOL InitOpenGL(HWND hWnd);
void DestroyOpenGL(HWND hWnd);
void Render(HWND hWnd);
void fnCheckGLError(const char* szFile, int nLine);
#define _CheckGLError_ fnCheckGLError(__FILE__,__LINE__);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int __stdcall WinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in_opt LPSTR lpCmdLine, __in int nShowCmd)
{
    BOOL bResult = FALSE;

    MSG msg = { 0 };
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszClassName = L"Outline04-BlurStencilWindowClass";
    wc.style = CS_OWNDC;
    if (!RegisterClass(&wc))
        return 1;
    HWND hWnd = CreateWindowW(wc.lpszClassName, L"Outline04-BlurStencil", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, 0, 0, hInstance, 0);

    ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);

    bResult = CreateOpenGLRenderContext(hWnd);
    if (bResult == FALSE)
    {
        OutputDebugStringA("CreateOpenGLRenderContext failed!\n");
        return 1;
    }
    InitGLEW();
    InitOpenGL(hWnd);

    // Set viewport according to the client rect
    if (!GetClientRect(hWnd, &clientRect))
    {
        OutputDebugString(L"GetClientRect Failed!\n");
        return FALSE;
    }
    clientWidth = clientRect.right - clientRect.left;
    clientHeight = clientRect.bottom - clientRect.top;
    glViewport(0, 0, GLsizei(clientWidth), GLsizei(clientHeight));

    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render(hWnd);
        }
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        DestroyOpenGL(hWnd);
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyOpenGL(hWnd);
            PostQuitMessage(0);
            break;
        }
    case WM_SIZE:
        clientWidth = LOWORD(lParam);
        clientHeight = HIWORD(lParam);
        glViewport(0, 0, clientWidth, clientHeight);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

BOOL InitGLEW()
{
    char buffer[128];
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        sprintf(buffer, "Error: %s\n", (char*)glewGetErrorString(err));
        OutputDebugStringA(buffer);
        return FALSE;
    }
    sprintf(buffer, "Status: Using GLEW %s\n", (char*)glewGetString(GLEW_VERSION));
    OutputDebugStringA(buffer);
    return TRUE;
}

BOOL CreateOpenGLRenderContext(HWND hWnd)
{
    BOOL bResult;
    char buffer[128];
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  //  size of this pfd
        1,                     // version number
        PFD_DRAW_TO_WINDOW |   // support window
        PFD_SUPPORT_OPENGL |   // support OpenGL
        PFD_DOUBLEBUFFER,      // double buffered
        PFD_TYPE_RGBA,         // RGBA type
        32,                    // 32-bit color depth
        0, 0, 0, 0, 0, 0,      // color bits ignored
        0,                     // no alpha buffer
        0,                     // shift bit ignored
        0,                     // no accumulation buffer
        0, 0, 0, 0,            // accum bits ignored
        24,                    // 24-bit z-buffer
        8,                     // 8-bit stencil buffer
        0,                     // no auxiliary buffer
        PFD_MAIN_PLANE,        // main layer
        0,                     // reserved
        0, 0, 0                // layer masks ignored
    };

    hDC = GetDC(hWnd);
    if (hDC == NULL)
    {
        OutputDebugStringA("Error: GetDC Failed!\n");
        return FALSE;
    }

    int pixelFormatIndex;
    pixelFormatIndex = ChoosePixelFormat(hDC, &pfd);
    if (pixelFormatIndex == 0)
    {
        sprintf(buffer, "Error %d: ChoosePixelFormat Failed!\n", GetLastError());
        OutputDebugStringA(buffer);
        return FALSE;
    }
    bResult = SetPixelFormat(hDC, pixelFormatIndex, &pfd);
    if (bResult == FALSE)
    {
        OutputDebugStringA("SetPixelFormat Failed!\n");
        return FALSE;
    }

    openGLRC = wglCreateContext(hDC);
    if (openGLRC == NULL)
    {
        sprintf(buffer, "Error %d: wglCreateContext Failed!\n", GetLastError());
        OutputDebugStringA(buffer);
        return FALSE;
    }
    bResult = wglMakeCurrent(hDC, openGLRC);
    if (bResult == FALSE)
    {
        sprintf(buffer, "Error %d: wglMakeCurrent Failed!\n", GetLastError());
        OutputDebugStringA(buffer);
        return FALSE;
    }

    sprintf(buffer, "OpenGL version info: %s\n", (char*)glGetString(GL_VERSION));
    OutputDebugStringA(buffer);
    return TRUE;
}

void fnCheckGLError(const char* szFile, int nLine)
{
    GLenum ErrCode = glGetError();
    if (GL_NO_ERROR != ErrCode)
    {
        const char* szErr = "GL_UNKNOWN ERROR";
        switch (ErrCode)
        {
        case GL_INVALID_ENUM:		szErr = "GL_INVALID_ENUM		";		break;
        case GL_INVALID_VALUE:		szErr = "GL_INVALID_VALUE		";		break;
        case GL_INVALID_OPERATION:	szErr = "GL_INVALID_OPERATION	";		break;
        case GL_OUT_OF_MEMORY:		szErr = "GL_OUT_OF_MEMORY		";		break;
        }
        char buffer[512];
        sprintf(buffer, "%s(%d):glError %s\n", szFile, nLine, szErr);
        OutputDebugStringA(buffer);
    }
}

float vertices[] = {
    // front
    -1.0, -1.0,  1.0,  0.0, 0.0,
     1.0, -1.0,  1.0,  1.0, 0.0,
     1.0,  1.0,  1.0,  1.0, 1.0,
    -1.0,  1.0,  1.0,  0.0, 1.0,
    // top
    -1.0,  1.0,  1.0,  0.0, 0.0,
     1.0,  1.0,  1.0,  1.0, 0.0,
     1.0,  1.0, -1.0,  1.0, 1.0,
    -1.0,  1.0, -1.0,  0.0, 1.0,
    // back
     1.0, -1.0, -1.0,  0.0, 0.0,
    -1.0, -1.0, -1.0,  1.0, 0.0,
    -1.0,  1.0, -1.0,  1.0, 1.0,
     1.0,  1.0, -1.0,  0.0, 1.0,
     // bottom
     -1.0, -1.0, -1.0,  0.0, 0.0,
      1.0, -1.0, -1.0,  1.0, 0.0,
      1.0, -1.0,  1.0,  1.0, 1.0,
     -1.0, -1.0,  1.0,  0.0, 1.0,
     // left
     -1.0, -1.0, -1.0,  0.0, 0.0,
     -1.0, -1.0,  1.0,  1.0, 0.0,
     -1.0,  1.0,  1.0,  1.0, 1.0,
     -1.0,  1.0, -1.0,  0.0, 1.0,
     // right
      1.0, -1.0,  1.0,  0.0, 0.0,
      1.0, -1.0, -1.0,  1.0, 0.0,
      1.0,  1.0, -1.0,  1.0, 1.0,
      1.0,  1.0,  1.0,  0.0, 1.0
};

//index buffer
unsigned int indices[]{
    // front
     0,  1,  2,
     2,  3,  0,
     // top
      4,  5,  6,
      6,  7,  4,
      // back
       8,  9, 10,
      10, 11,  8,
      // bottom
      12, 13, 14,
      14, 15, 12,
      // left
      16, 17, 18,
      18, 19, 16,
      // right
      20, 21, 22,
      22, 23, 20,
};

BOOL InitOpenGL(HWND hWnd)
{
    GLint compiled;
    //normal model program
    {
        //Vertex shader
        const char* vShaderStr = R"(
#version 330
uniform mat4 ProjMtx;
layout (location = 0) in vec4 in_Position;
layout (location = 1) in vec2 in_TexCoord;
out vec2 out_TexCoord;
void main()
{
	out_TexCoord = in_TexCoord;
	gl_Position = ProjMtx * in_Position;
}
)";
        vShader = glCreateShader(GL_VERTEX_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return -1;
        }
        glShaderSource(vShader, 1, &vShaderStr, nullptr);
        glCompileShader(vShader);
        glGetShaderiv(vShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(vShader, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error compiling vertex shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(vShader);
            return -1;
        }

        //Fragment shader
        const char* pShaderStr = R"(
#version 330
uniform sampler2D Texture;
in vec2 out_TexCoord;
out vec4 Out_Color;
void main()
{
	vec2 st = vec2(out_TexCoord.s, 1-out_TexCoord.t);//lodePNG decoded pixels is upside-down for OpenGL
	Out_Color = texture(Texture, st);
}
)";
        pShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return FALSE;
        }
        glShaderSource(pShader, 1, &pShaderStr, nullptr);
        glCompileShader(pShader);
        glGetShaderiv(pShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(pShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(pShader, infoLength, NULL, infoLog);
                OutputDebugString(L"Error compiling fragment shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(pShader);
            return FALSE;
        }

        //Program
        GLint linked;
        GLuint program = glCreateProgram();
        if (program == 0)
        {
            OutputDebugString(L"glCreateProgram Failed!\n");
            return -1;
        }
        glAttachShader(program, vShader);
        glAttachShader(program, pShader);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            GLint infoLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char) * infoLength);
                glGetProgramInfoLog(program, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error Linking program: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteProgram(program);
            return FALSE;
        }
        normalProgram = program;

        //uniform location
        glUseProgram(normalProgram);
        uniformProjMtx = glGetUniformLocation(normalProgram, "ProjMtx");
    }


    //flat color model program
    {
        //Vertex shader
        const char* vShaderStr = R"(
#version 330
uniform mat4 ProjMtx;
layout (location = 0) in vec4 in_Position;
layout (location = 1) in vec2 in_TexCoord;
void main()
{
	gl_Position = ProjMtx * in_Position;
}
)";
        vShader = glCreateShader(GL_VERTEX_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return -1;
        }
        glShaderSource(vShader, 1, &vShaderStr, nullptr);
        glCompileShader(vShader);
        glGetShaderiv(vShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(vShader, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error compiling vertex shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(vShader);
            return -1;
        }

        //Fragment shader
        const char* pShaderStr = R"(
#version 330
out vec4 Out_Color;
void main()
{
	Out_Color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}
)";
        pShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return FALSE;
        }
        glShaderSource(pShader, 1, &pShaderStr, nullptr);
        glCompileShader(pShader);
        glGetShaderiv(pShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(pShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(pShader, infoLength, NULL, infoLog);
                OutputDebugString(L"Error compiling fragment shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(pShader);
            return FALSE;
        }

        //Program
        GLint linked;
        GLuint program = glCreateProgram();
        if (program == 0)
        {
            OutputDebugString(L"glCreateProgram Failed!\n");
            return -1;
        }
        glAttachShader(program, vShader);
        glAttachShader(program, pShader);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            GLint infoLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char) * infoLength);
                glGetProgramInfoLog(program, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error Linking program: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteProgram(program);
            return FALSE;
        }
        flatColorProgram = program;
    }

    //blur program for framebuffer quad
    {
        //Vertex shader
        const char* vShaderStr = R"(
#version 330
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoords = aTexCoords;
}
)";
        vShader = glCreateShader(GL_VERTEX_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return -1;
        }
        glShaderSource(vShader, 1, &vShaderStr, nullptr);
        glCompileShader(vShader);
        glGetShaderiv(vShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(vShader, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error compiling vertex shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(vShader);
            return -1;
        }

        //Fragment shader
        const char* pShaderStr = R"(
#version 330
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D screenTexture;
const float offset = 1.0 / 300.0;
void main()
{
    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // top-left
        vec2( 0.0f,    offset), // top-center
        vec2( offset,  offset), // top-right
        vec2(-offset,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( offset,  0.0f),   // center-right
        vec2(-offset, -offset), // bottom-left
        vec2( 0.0f,   -offset), // bottom-center
        vec2( offset, -offset)  // bottom-right
    );

    float kernel[9] = float[](
    1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);

    vec4 sampleTex[9];
    for(int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec4(texture(screenTexture, TexCoords.st + offsets[i]));
    }
    vec4 col = vec4(0.0,0.0,0.0,1.0);
    for(int i = 0; i < 9; i++)
        col += sampleTex[i] * kernel[i];

    FragColor = col;
}
)";
        pShader = glCreateShader(GL_FRAGMENT_SHADER);
        if (vShader == 0)
        {
            OutputDebugString(L"glCreateShader Failed!\n");
            return FALSE;
        }
        glShaderSource(pShader, 1, &pShaderStr, nullptr);
        glCompileShader(pShader);
        glGetShaderiv(pShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLength = 0;
            glGetShaderiv(pShader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char)*infoLength);
                glGetShaderInfoLog(pShader, infoLength, NULL, infoLog);
                OutputDebugString(L"Error compiling fragment shader: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteShader(pShader);
            return FALSE;
        }

        //Program
        GLint linked;
        GLuint program = glCreateProgram();
        if (program == 0)
        {
            OutputDebugString(L"glCreateProgram Failed!\n");
            return -1;
        }
        glAttachShader(program, vShader);
        glAttachShader(program, pShader);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            GLint infoLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength > 1)
            {
                char* infoLog = (char*)malloc(sizeof(char) * infoLength);
                glGetProgramInfoLog(program, infoLength, nullptr, infoLog);
                OutputDebugString(L"Error Linking program: \n");
                OutputDebugStringA(infoLog);
                OutputDebugStringA("\n");
                free(infoLog);
            }
            glDeleteProgram(program);
            return FALSE;
        }
        blurProgram = program;
    }

    //vertex buffer
    glGenBuffers(1, &vertexBuf);
    assert(vertexBuf != 0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (GLvoid*)vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &indexBuf);
    assert(indexBuf != 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (GLvoid*)indices, GL_STATIC_DRAW);

    glUseProgram(normalProgram);
    //attribute locations
    attributePos = 0;
    attributeTexCoord = 1;
    //set attribute of positon and texcoord
    glVertexAttribPointer(attributePos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(attributeTexCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    //enable attribute array
    glEnableVertexAttribArray(attributePos);
    glEnableVertexAttribArray(attributeTexCoord);

    //texture
    unsigned error;

    error = lodepng_decode32_file(&image, &width, &height, "CheckerMap.png");
    if (error)
    {
        char buf[128];
        sprintf(buf, "error %u: %s\n", error, lodepng_error_text(error));
        OutputDebugStringA(buf);
    }

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    free(image);

    //sampler settings
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    //other settings
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    _CheckGLError_;

    //set up frame buffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    //generate texture for color
    glGenTextures(1, &textureColorBuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, clientWidth, clientHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    //attach it to currently bound framebuffer object
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorBuffer, 0);
    _CheckGLError_;
    //generate render buffer for depth and stencil
    glGenRenderbuffers(1, &renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, clientWidth, clientHeight);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    //attach it to currently bound framebuffer object
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        char buf[128];
        sprintf(buf, "error: Framebuffer is not complete.\n");
        OutputDebugStringA(buf);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glUseProgram(blurProgram);
    attributePosQuad = 0;
    attributeTexCoordQuad = 1;
    glEnableVertexAttribArray(attributePosQuad);
    glVertexAttribPointer(attributePosQuad, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(attributeTexCoordQuad);
    glVertexAttribPointer(attributeTexCoordQuad, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    _CheckGLError_;

    return TRUE;
}

void DestroyOpenGL(HWND hWnd)
{
    //OpenGL Destroy
    glDeleteShader(vShader);
    glDeleteShader(pShader);
    glDeleteProgram(normalProgram);

    glDeleteProgram(blurProgram);
    glDeleteProgram(flatColorProgram);

    //OpenGL Context Destroy
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(openGLRC);
    ReleaseDC(hWnd, hDC);
}

void setUniformMVP(GLuint Location, float ratio, int rotationSpeed, glm::vec3 modelOffset, glm::vec3 const& modelScale)
{
    glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    model = glm::translate(model, modelOffset);
    model = glm::rotate(model, glm::radians(1.0f*rotationSpeed), glm::vec3(1.0f, 1.0f, 0.0f));
    model = glm::scale(model, modelScale);

    view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    glm::mat4 MVP = projection * view * model;
    glUniformMatrix4fv(Location, 1, GL_FALSE, glm::value_ptr(MVP));
}

int t = 360;

void Render(HWND hWnd)
{
    t--;
    if (t == 0)
    {
        t = 360;
    }

    //clear all
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

#if true//draw textured box to default framebuffer, set valid fragments' stencil to 0xFF
    //bind to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //render the box to set the stencil value to 0xFF
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF); // all fragments should update the stencil buffer
    glStencilMask(0xFF); // enable writing to the stencil buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUseProgram(normalProgram);//use textured shader
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif

#if true//draw flat-colored box to the framebuffer
    //bind to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    //render the box with a flat color
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glStencilMask(0x00); // disable writing to the stencil buffer
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUseProgram(flatColorProgram);//use flat color shader
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(0, 0, 0), glm::vec3(1, 1, 1));
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
#endif

#if true//draw the blurred framebuffer quad to default framebuffer
    //bind to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glDisable(GL_DEPTH_TEST);// disable depth test so screen-space quad isn't discarded due to depth test.
    glEnable(GL_STENCIL_TEST);

    //Only fragments whose stencil values are not equal to 1 will be drawn.
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00); // disable writing to the stencil buffer

    //draw a quad plane with the attached framebuffer color texture
    glUseProgram(blurProgram);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureColorBuffer);// use the color attachment texture as the texture of the quad plane
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glStencilMask(0xFF);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif

    _CheckGLError_

    SwapBuffers(hDC);
}