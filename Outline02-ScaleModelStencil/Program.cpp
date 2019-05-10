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
GLuint programs[] = { 0,0 };
GLint attributePos = 0;
GLint attributeTexCoord = 0;
GLint uniformProjMtx = 0;
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
    wc.lpszClassName = L"Outline02-ScaleModelStencilWindowClass";
    wc.style = CS_OWNDC;
    if (!RegisterClass(&wc))
        return 1;
    HWND hWnd = CreateWindowW(wc.lpszClassName, L"Outline02-ScaleModelStencil", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, 0, 0, hInstance, 0);

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

GLuint CreateShaderProgram(const char* vShaderStr, const char* pShaderStr)
{
    GLint compiled;
    //Vertex shader
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
    pShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (vShader == 0)
    {
        OutputDebugString(L"glCreateShader Failed!\n");
        return -1;
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
        return -1;
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
        return -1;
    }
    return program;
}

BOOL InitOpenGL(HWND hWnd)
{
    GLint compiled;
    {
        const char* vShaderStr = R"(
#version 330
uniform mat4 ProjMtx;
in vec4 in_Position;
in vec2 in_TexCoord;
out vec2 out_TexCoord;
void main()
{
	out_TexCoord = in_TexCoord;
	gl_Position = ProjMtx * in_Position;
}
)";
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
        programs[0] = CreateShaderProgram(vShaderStr, pShaderStr);
    }
    _CheckGLError_

    {
        const char* vShaderStr = R"(
#version 330
uniform mat4 ProjMtx;
in vec4 in_Position;
void main()
{
	gl_Position = ProjMtx * in_Position;
}
)";
        const char* pShaderStr = R"(
#version 330
out vec4 Out_Color;
void main()
{
	Out_Color = vec4(1.0f, 0, 0, 1.0f);
}
)";
        programs[1] = CreateShaderProgram(vShaderStr, pShaderStr);
    }
    _CheckGLError_

    //get attribute and uniform location by name
    attributePos = glGetAttribLocation(programs[0], "in_Position");//get location of attribute <in_Position>
    attributeTexCoord = glGetAttribLocation(programs[0], "in_TexCoord");//get location of attribute <in_TexCoord>
    uniformProjMtx = glGetUniformLocation(programs[0], "ProjMtx");

    _CheckGLError_

    //vertex buffer
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
    glGenBuffers(1, &vertexBuf);
    assert(vertexBuf != 0);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (GLvoid*)vertices, GL_STATIC_DRAW);

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
    glGenBuffers(1, &indexBuf);
    assert(indexBuf != 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (GLvoid*)indices, GL_STATIC_DRAW);

    //set attribute of positon and texcoord
    glVertexAttribPointer(attributePos, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
    glVertexAttribPointer(attributeTexCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    //enable attribute array
    glEnableVertexAttribArray(attributePos);
    glEnableVertexAttribArray(attributeTexCoord);

    _CheckGLError_

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
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    _CheckGLError_

    return TRUE;
}

void DestroyOpenGL(HWND hWnd)
{
    //OpenGL Destroy
    glDeleteShader(vShader);
    glDeleteShader(pShader);
    glDeleteProgram(programs[0]);
    glDeleteProgram(programs[1]);

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

    // clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // bind vbo
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuf);

    // bind texture
    glBindTexture(GL_TEXTURE_2D, texture);

    //Set the stencil func to GL_ALWAYS before drawing the (to be outlined) objects,
    //updating the stencil buffer with 1s wherever the objects' fragments are rendered.
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF); // all fragments should update the stencil buffer
    glStencilMask(0xFF); // enable writing to the stencil buffer
    //draw the object with normal shader
    glUseProgram(programs[0]);
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1, 1, 1));//not scaled
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1, 1, 1));
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

    //Only fragments whose stencil values are not equal to 1 will be drawn.
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilMask(0x00); // disable writing to the stencil buffer
    glDisable(GL_DEPTH_TEST);//drawing outline, should b
    //draw the object with single color shader
    glUseProgram(programs[1]);
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.06, 1.06, 1.06));//scaled up
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    setUniformMVP(uniformProjMtx, (float)clientWidth / clientHeight, t, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.06, 1.06, 1.06));//scaled up
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    //restore stencil mask and enable depth test
    glStencilMask(0xFF);
    glEnable(GL_DEPTH_TEST);

    _CheckGLError_

    SwapBuffers(hDC);
}