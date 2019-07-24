
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>

#ifdef __AVX__
#undef __AVX__
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "videodevice.h"
#include "result.h"

namespace {
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
    PFNGLUSEPROGRAMPROC glUseProgram;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLUNIFORM1IPROC glUniform1i;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    PFNGLBUFFERDATAPROC glBufferData;

    void InitGLext()
    {
        glCreateShader = (PFNGLCREATESHADERPROC) glXGetProcAddress((const GLubyte*) "glCreateShader");
        glGetProgramiv = (PFNGLGETPROGRAMIVPROC) glXGetProcAddress((const GLubyte*) "glGetProgramiv");
        glShaderSource = (PFNGLSHADERSOURCEPROC) glXGetProcAddress((const GLubyte*) "glShaderSource");
        glCompileShader = (PFNGLCOMPILESHADERPROC) glXGetProcAddress((const GLubyte*) "glCompileShader");
        glGetShaderiv = (PFNGLGETSHADERIVPROC) glXGetProcAddress((const GLubyte*) "glGetShaderiv");
        glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) glXGetProcAddress((const GLubyte*) "glGetShaderInfoLog");
        glCreateProgram = (PFNGLCREATEPROGRAMPROC) glXGetProcAddress((const GLubyte*) "glCreateProgram");
        glAttachShader = (PFNGLATTACHSHADERPROC) glXGetProcAddress((const GLubyte*) "glAttachShader");
        glLinkProgram = (PFNGLLINKPROGRAMPROC) glXGetProcAddress((const GLubyte*) "glLinkProgram");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) glXGetProcAddress((const GLubyte*) "glGetUniformLocation");
        glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) glXGetProcAddress((const GLubyte*) "glGetAttribLocation");
        glUseProgram = (PFNGLUSEPROGRAMPROC) glXGetProcAddress((const GLubyte*) "glUseProgram");
        glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) glXGetProcAddress((const GLubyte*) "glGenVertexArrays");
        glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) glXGetProcAddress((const GLubyte*) "glBindVertexArray");
        glGenBuffers = (PFNGLGENBUFFERSPROC) glXGetProcAddress((const GLubyte*) "glGenBuffers");
        glBindBuffer = (PFNGLBINDBUFFERPROC) glXGetProcAddress((const GLubyte*) "glBindBuffer");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) glXGetProcAddress((const GLubyte*) "glVertexAttribPointer");
        glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) glXGetProcAddress((const GLubyte*) "glEnableVertexAttribArray");
        glUniform1i = (PFNGLUNIFORM1IPROC) glXGetProcAddress((const GLubyte*) "glUniform1i");
        glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) glXGetProcAddress((const GLubyte*) "glUniformMatrix4fv");
        glBufferData = (PFNGLBUFFERDATAPROC) glXGetProcAddress((const GLubyte*) "glBufferData");
    }

    Result BuildShader(std::string const &shaderSource, GLuint &shader, GLenum type) {
        Result result;
        int32_t size = shaderSource.length();
        shader = glCreateShader(type);
        char const *cShaderSource = shaderSource.c_str();
        glShaderSource(shader, 1, (GLchar const **)&cShaderSource, &size);
        glCompileShader(shader);
        GLint status = 0;;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            int32_t length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

            char *log = new char[length];
            glGetShaderInfoLog(shader, length, &length, log);

            result = Result(false, "Device failed to compiled shader %s", log);

            delete[] log;
        }
        
        return result;
    }

    Result BuildProgram(videodevice::Device* device)
    {
        Result result;

        const std::string vertexShaderSource = 
        "#version 150\n"
        "in vec3 vertex;\n"
        "in vec2 texCoord0;\n"
        "uniform mat4 mvpMatrix;\n"
        "out vec2 texCoord;\n"
        "void main() {\n"
        "	gl_Position = mvpMatrix * vec4(vertex, 1.0);\n"
        "	texCoord = texCoord0;\n"
        "}\n";

        const std::string fragmentShaderSource =
        "#version 150\n"
        "uniform sampler2D frameTex;\n"
        "in vec2 texCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "	fragColor = texture(frameTex, texCoord);\n"
        "}\n";


        GLuint vertexShader, fragmentShader;
        result = BuildShader(vertexShaderSource, vertexShader, GL_VERTEX_SHADER);
        if(!result)
        {
            return result;
        }
        
        result = BuildShader(fragmentShaderSource, fragmentShader, GL_FRAGMENT_SHADER);
        if(!result)
        {
            return result;
        }
        
        device->program = glCreateProgram();
        glAttachShader(device->program, vertexShader);
        glAttachShader(device->program, fragmentShader);
        glLinkProgram(device->program);
        GLint status;
        glGetProgramiv(device->program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            int32_t length;
            glGetProgramiv(device->program, GL_INFO_LOG_LENGTH, &length);
            
            char *log = new char[length];
            glGetShaderInfoLog(device->program, length, &length, log);

            result = Result(false, "DeviceBuildProgram failed to link program %s", log);
            delete[] log;

            return result;
        }
        
        device->uniforms[videodevice::Device::MVP_MATRIX] = glGetUniformLocation(device->program, "mvpMatrix");
        device->uniforms[videodevice::Device::FRAME_TEX] = glGetUniformLocation(device->program, "frameTex");
        
        device->attribs[videodevice::Device::VERTICES] = glGetAttribLocation(device->program, "vertex");
        device->attribs[videodevice::Device::TEX_COORDS] = glGetAttribLocation(device->program, "texCoord0");

        return result;
    }


    void WriteVertexBuffer(videodevice::Device* device, uint32_t width, uint32_t height)
    {
        const float h = static_cast<float>(height);
        const float w = static_cast<float>(width);

        glBindBuffer(GL_ARRAY_BUFFER, device->vertexBuffer);
        float quad[20] = {
            // x      y     z     u      v
             0.0f ,   h,   0.0f, 0.0f, 0.0f,
             0.0f,   0.0f, 0.0f, 0.0f, 1.0f,
              w,     0.0f, 0.0f, 1.0f, 1.0f,
              w,      h,   0.0f, 1.0f, 0.0f
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    }

    void WriteMVPMatrix(videodevice::Device* device, uint32_t width, uint32_t height)
    {
        glm::mat4 mvp = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);
        glUniformMatrix4fv(device->uniforms[videodevice::Device::MVP_MATRIX], 1, GL_FALSE, glm::value_ptr(mvp));
    }
}

namespace videodevice
{
    Result Init()
    {
        InitGLext();
        return Result();
    }

    Result Create(Device*& device, uint32_t width, uint32_t height)
    {
        Result result;

        device = new Device();
        memset(device, 0, sizeof(Device));

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glEnable(GL_TEXTURE_2D);

        result = BuildProgram(device);
        if(!result)
        {
            return result;
        }

        glUseProgram(device->program);

        // initialize renderable

        // vertexArray
        glGenVertexArrays(1, &device->vertexArray);
        glBindVertexArray(device->vertexArray);
        
        // vertexBuffer
        glGenBuffers(1, &device->vertexBuffer);
        WriteVertexBuffer(device, width, height);

        glVertexAttribPointer(device->attribs[Device::VERTICES], 3, GL_FLOAT, GL_FALSE, 20, ((char *)NULL + (0)));
        glEnableVertexAttribArray(device->attribs[Device::VERTICES]);

        glVertexAttribPointer(device->attribs[Device::TEX_COORDS], 2, GL_FLOAT, GL_FALSE, 20,  ((char *)NULL + (12)));
        glEnableVertexAttribArray(device->attribs[Device::TEX_COORDS]);  

        glGenBuffers(1, &device->elementBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device->elementBuffer);
        unsigned char elem[6] = {
            0, 1, 2,
            0, 2, 3
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elem), elem, GL_STATIC_DRAW);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &device->frameTexture);
        glBindTexture(GL_TEXTURE_2D, device->frameTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 
            0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glUniform1i(device->uniforms[Device::FRAME_TEX], 0);
        
        WriteMVPMatrix(device, width, height);

        return result;
    }

    Result DrawFrame(Device* device, uint8_t* f, uint32_t width, uint32_t height)
    {
        Result result;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, 
                        height, GL_RGB, GL_UNSIGNED_BYTE, f);

        glClear(GL_COLOR_BUFFER_BIT);	
        glBindTexture(GL_TEXTURE_2D, device->frameTexture);
        glBindVertexArray(device->vertexArray);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, ((char *)NULL + (0)));
        glBindVertexArray(0);
        return result;
    }

    Result SetSize(Device* device, uint32_t width, uint32_t height)
    {
        Result result;

        glViewport(0,0, width, height);

        WriteVertexBuffer(device, width, height);
        WriteMVPMatrix(device, width, height);

        return result;
    }

}




