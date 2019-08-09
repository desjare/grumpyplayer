#include "precomp.h"
#include "videodevice.h"
#include "result.h"
#include "numeric.h"
#include "logger.h"


#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// platform specific includes
#ifdef UNIX
#include <GL/glx.h>
#define vdGetProcAddress glXGetProcAddress
#define PROCADDRNAMEPTR const GLubyte*
#elif defined(WIN32)
#include <gl/wglext.h>
#define vdGetProcAddress wglGetProcAddress
#define PROCADDRNAMEPTR LPCSTR
#endif

#ifdef __AVX__
#undef __AVX__
#endif

#include <algorithm>

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
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;

    videodevice::Device* currentDevice = NULL;

    void CheckOpenGLError(const char* stmt, const char* fname, int line)
    {
        bool error = false;
        GLenum err;
        while( (err = glGetError()) != GL_NO_ERROR)
        {
            logger::Error("OpenGL error %08x, %s at %s:%i - for %s\n", err, gluErrorString(err), fname, line, stmt);
            error = true;
        }

        if(error)
        {
            #ifdef _DEBUG
            assert(!error);
            #else
            abort();
            #endif
        }
    }

    #define GL_CHECK(stmt)\
        do { \
            stmt; \
            CheckOpenGLError(#stmt, __FILE__, __LINE__); \
        } while (0)


    // GetProcAddress
    typedef void(*Proc)();

    Proc GetProcAddress(PROCADDRNAMEPTR n )
    {
        Proc addr = vdGetProcAddress(n);
        if(!addr) 
        {
            logger::Error("GetProcAddress %s not found!", reinterpret_cast<const char*>(n));
        }
        assert(addr);
        return addr;
    }


    void InitGLext()
    {
        glCreateShader = (PFNGLCREATESHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glCreateShader");
        glGetProgramiv = (PFNGLGETPROGRAMIVPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetProgramiv");
        glShaderSource = (PFNGLSHADERSOURCEPROC) GetProcAddress((PROCADDRNAMEPTR) "glShaderSource");
        glCompileShader = (PFNGLCOMPILESHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glCompileShader");
        glGetShaderiv = (PFNGLGETSHADERIVPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetShaderiv");
        glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetShaderInfoLog");
        glCreateProgram = (PFNGLCREATEPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glCreateProgram");
        glAttachShader = (PFNGLATTACHSHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glAttachShader");
        glLinkProgram = (PFNGLLINKPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glLinkProgram");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetUniformLocation");
        glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetAttribLocation");
        glUseProgram = (PFNGLUSEPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glUseProgram");
        glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) GetProcAddress((PROCADDRNAMEPTR) "glGenVertexArrays");
        glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) GetProcAddress((PROCADDRNAMEPTR) "glBindVertexArray");
        glGenBuffers = (PFNGLGENBUFFERSPROC) GetProcAddress((PROCADDRNAMEPTR) "glGenBuffers");
        glBindBuffer = (PFNGLBINDBUFFERPROC) GetProcAddress((PROCADDRNAMEPTR) "glBindBuffer");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) GetProcAddress((PROCADDRNAMEPTR) "glVertexAttribPointer");
        glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) GetProcAddress((PROCADDRNAMEPTR) "glEnableVertexAttribArray");
        glUniform1i = (PFNGLUNIFORM1IPROC) GetProcAddress((PROCADDRNAMEPTR) "glUniform1i");
        glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) GetProcAddress((PROCADDRNAMEPTR) "glUniformMatrix4fv");
        glBufferData = (PFNGLBUFFERDATAPROC) GetProcAddress((PROCADDRNAMEPTR) "glBufferData");
        glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) GetProcAddress((PROCADDRNAMEPTR) "glDeleteVertexArrays");
        glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) GetProcAddress((PROCADDRNAMEPTR) "glDeleteBuffers");
    }

    Result BuildShader(std::string const &shaderSource, GLuint &shader, GLenum type) {
        Result result;
        int32_t size = shaderSource.length();
        GL_CHECK(shader = glCreateShader(type));
        char const *cShaderSource = shaderSource.c_str();
        GL_CHECK(glShaderSource(shader, 1, (GLchar const **)&cShaderSource, &size));
        GL_CHECK(glCompileShader(shader));
        GLint status = 0;;
        GL_CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
        if (status != GL_TRUE) {
            int32_t length = 0;
            GL_CHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

            char *log = new char[length];
            GL_CHECK(glGetShaderInfoLog(shader, length, &length, log));

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
        "vec4 cubic(float v){\n"
        "    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;\n"
        "    vec4 s = n * n * n;\n"
        "    float x = s.x;\n"
        "    float y = s.y - 4.0 * s.x;\n"
        "    float z = s.z - 4.0 * s.y + 6.0 * s.x;\n"
        "    float w = 6.0 - x - y - z;\n"
        "    return vec4(x, y, z, w) * (1.0/6.0);\n"
        "}\n"
        "vec4 textureBicubic(sampler2D sampler, vec2 texCoords){\n"
        "\n"
        "   vec2 texSize = textureSize(sampler, 0);\n"
        "   vec2 invTexSize = 1.0 / texSize;\n"
        "\n"
        "   texCoords = texCoords * texSize - 0.5;\n"
        "\n"
        "\n"
        "    vec2 fxy = fract(texCoords);\n"
        "    texCoords -= fxy;\n"
        "\n"
        "    vec4 xcubic = cubic(fxy.x);\n"
        "    vec4 ycubic = cubic(fxy.y);\n"
        "\n"
        "    vec4 c = texCoords.xxyy + vec2 (-0.5, +1.5).xyxy;\n"
        "\n"
        "    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);\n"
        "    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;\n"
        "\n"
        "    offset *= invTexSize.xxyy;\n"
        "\n"
        "    vec4 sample0 = texture(sampler, offset.xz);\n"
        "    vec4 sample1 = texture(sampler, offset.yz);\n"
        "    vec4 sample2 = texture(sampler, offset.xw);\n"
        "    vec4 sample3 = texture(sampler, offset.yw);\n"
        "\n"
        "    float sx = s.x / (s.x + s.y);\n"
        "    float sy = s.z / (s.z + s.w);\n"
        "\n"
        "    return mix(\n"
        "       mix(sample3, sample2, sx), mix(sample1, sample0, sx)\n"
        "    , sy);\n"
        "}\n"
        "void main() {\n"
        "	fragColor = textureBicubic(frameTex, texCoord);\n"
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
        
        GL_CHECK(device->program = glCreateProgram());
        GL_CHECK(glAttachShader(device->program, vertexShader));
        GL_CHECK(glAttachShader(device->program, fragmentShader));
        GL_CHECK(glLinkProgram(device->program));
        GLint status;
        GL_CHECK(glGetProgramiv(device->program, GL_LINK_STATUS, &status));
        if (status != GL_TRUE) {
            int32_t length;
            GL_CHECK(glGetProgramiv(device->program, GL_INFO_LOG_LENGTH, &length));
            
            char *log = new char[length];
            GL_CHECK(glGetShaderInfoLog(device->program, length, &length, log));

            result = Result(false, "DeviceBuildProgram failed to link program %s", log);
            delete[] log;

            return result;
        }
        
        GL_CHECK(device->uniforms[videodevice::Device::MVP_MATRIX] = glGetUniformLocation(device->program, "mvpMatrix"));
        GL_CHECK(device->uniforms[videodevice::Device::FRAME_TEX] = glGetUniformLocation(device->program, "frameTex"));
        
        GL_CHECK(device->attribs[videodevice::Device::VERTICES] = glGetAttribLocation(device->program, "vertex"));
        GL_CHECK(device->attribs[videodevice::Device::TEX_COORDS] = glGetAttribLocation(device->program, "texCoord0"));

        return result;
    }

    void WriteVertexBuffer(videodevice::Device* device, float x1, float y1, float x2, float y2)
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, device->vertexBuffer));
        float quad[20] = {
            // x      y     z     u      v
              x1,   y2,   0.0f, 0.0f, 0.0f,
              x1,   y1,   0.0f, 0.0f, 1.0f,
              x2,   y1,   0.0f, 1.0f, 1.0f,
              x2,   y2,  0.0f, 1.0f, 0.0f
        };
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW));
    }

    void WriteMVPMatrix(videodevice::Device* device, uint32_t width, uint32_t height)
    {
        glm::mat4 mvp = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);
        GL_CHECK(glUniformMatrix4fv(device->uniforms[videodevice::Device::MVP_MATRIX], 1, GL_FALSE, glm::value_ptr(mvp)));
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

        if( currentDevice != NULL )
        {
            return Result(false, "Video device already exist. Cannot create more than one device.");
        }

        device = new Device();
        memset(device, 0, sizeof(Device));

        currentDevice = device;

        device->width = width;
        device->height = height;

        result = BuildProgram(device);
        if(!result)
        {
            return result;
        }

        GL_CHECK(glUseProgram(device->program));

        // initialize renderable
        GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

        // vertexArray
        GL_CHECK(glGenVertexArrays(1, &device->vertexArray));
        GL_CHECK(glBindVertexArray(device->vertexArray));
        
        // vertexBuffer
        GL_CHECK(glGenBuffers(1, &device->vertexBuffer));
        WriteVertexBuffer(device, 0, 0, width, height);

        GL_CHECK(glVertexAttribPointer(device->attribs[Device::VERTICES], 3, GL_FLOAT, GL_FALSE, 20, ((char *)NULL + (0))));
        GL_CHECK(glEnableVertexAttribArray(device->attribs[Device::VERTICES]));

        GL_CHECK(glVertexAttribPointer(device->attribs[Device::TEX_COORDS], 2, GL_FLOAT, GL_FALSE, 20,  ((char *)NULL + (12))));
        GL_CHECK(glEnableVertexAttribArray(device->attribs[Device::TEX_COORDS]));  

        GL_CHECK(glGenBuffers(1, &device->elementBuffer));
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, device->elementBuffer));
        unsigned char elem[6] = {
            0, 1, 2,
            0, 2, 3
        };
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elem), elem, GL_STATIC_DRAW));
        GL_CHECK(glBindVertexArray(0));

#ifndef WIN32 // FIXME TBM_desjare
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
#endif
        GL_CHECK(glGenTextures(1, &device->frameTexture));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, device->frameTexture));
        GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 
            0, GL_RGB, GL_UNSIGNED_BYTE, NULL));
        GL_CHECK(glUniform1i(device->uniforms[Device::FRAME_TEX], 0));
        
        WriteMVPMatrix(device, width, height);

        return result;
    }

    Result DrawFrame(Device* device, uint8_t* f, uint32_t width, uint32_t height)
    {
        Result result;
        GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, 
                        height, GL_RGB, GL_UNSIGNED_BYTE, f));

        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));	
        GL_CHECK(glBindTexture(GL_TEXTURE_2D, device->frameTexture));
        GL_CHECK(glBindVertexArray(device->vertexArray));
        GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, ((char *)NULL + (0))));
        GL_CHECK(glBindVertexArray(0));
        return result;
    }

    Result SetVideoSize(Device* device, uint32_t width, uint32_t height)
    {
        Result result;

        device->width = width;
        device->height = height;

        GL_CHECK(glBindTexture(GL_TEXTURE_2D, device->frameTexture));
        GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 
            0, GL_RGB, GL_UNSIGNED_BYTE, NULL));
        GL_CHECK(glUniform1i(device->uniforms[Device::FRAME_TEX], 0));

        return result;
    }

    Result SetWindowSize(Device* device, uint32_t width, uint32_t height)
    {
        Result result;

        const float ww = static_cast<float>(width);
        const float wh = static_cast<float>(height);
        const float tw = static_cast<float>(device->width);
        const float th = static_cast<float>(device->height);
        const float tr = tw / th;

        const float maxw = std::max(ww,tw);
        const float maxh = std::max(wh,th);

        float adjustWidth = ww;
        float adjustHeight = wh;
        float adjustRatio = 0.0f;

        if( maxw > maxh )
        {
            float w = maxw;
            float h = w / tr;

            if( h > maxh )
            {
                h = maxh;
                w = h * tr;
            }

            adjustWidth = w;
            adjustHeight = h;
        }
        else
        {
            float h = maxh;
            float w = h * tr;

            if( w > maxw )
            {
                w = maxw;
                h = h * tr;
            }

            adjustWidth = w;
            adjustHeight = h;
        }

        adjustRatio = adjustWidth / adjustHeight;

        if( !numeric::equals(tr, adjustRatio) )
        {
            logger::Debug("ratio differ");
            if( adjustRatio < 1.0f )
            {
                adjustWidth = ww;
                adjustHeight = adjustWidth / tr;
            }
        }



        float x1 = ww / 2.0f - adjustWidth / 2.0f;
        float x2 = x1 + adjustWidth;
        float y1 = wh / 2.0f - adjustHeight / 2.0f;
        float y2 = y1 + adjustHeight;

        logger::Debug("w %f h %f adjustWidth %f adjustHeight %f tr %f ar %f x1 %f x2 %f y1 %f y2 %f", ww, wh, adjustWidth, adjustHeight, tr, adjustWidth / adjustHeight, x1, x2, y1, y2);

        GL_CHECK(glViewport(0,0, width, height));

        WriteVertexBuffer(device, x1, y1, x2, y2);
        WriteMVPMatrix(device, width, height);

        return result;
    }

    void Destroy(Device* device)
    {
        GL_CHECK(glDeleteVertexArrays(1, &device->vertexArray));
     	GL_CHECK(glDeleteBuffers(1, &device->vertexBuffer));
    	GL_CHECK(glDeleteBuffers(1, &device->elementBuffer));
	    GL_CHECK(glDeleteTextures(1, &device->frameTexture));
        delete device;
        currentDevice = NULL;
    }

}




