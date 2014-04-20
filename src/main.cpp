#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include <stdlib.h>
#include <stdio.h>

#include "gfx/program.hpp"
#include "gfx/geometry.hpp"

#include "fs/folderwatcher.hpp"

static void error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

int main()
{
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit())
        exit(EXIT_FAILURE);
    
    glfwWindowHint(GLFW_VISIBLE, false);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
    GLFWwindow* window = glfwCreateWindow(1280, 720, "sandvox", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    
    glfwMakeContextCurrent(window);
    
    glfwShowWindow(window);
    glfwSetKeyCallback(window, key_callback);
    
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    FolderWatcher fw("../..");
    ProgramManager pm("../../src/shaders", &fw);
    
    shared_ptr<Buffer> vb = make_shared<Buffer>(Buffer::Type_Vertex, 12, 3, Buffer::Usage_Static);
    
    vec3* vptr = vb->lock<vec3>();
    vptr[0] = vec3(-1, -1, 0);
    vptr[1] = vec3(+3, -1, 0);
    vptr[2] = vec3(-1, +3, 0);
    vb->unlock();
    
    Geometry geom({ Geometry::Element(0, Geometry::Format_Float3) }, vb);
    
    while (!glfwWindowShouldClose(window))
    {
        fw.processChanges();
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        glViewport(0, 0, width, height);
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        if (Program* prog = pm.get("fullscreen-vs", "fullscreen-fs"))
        {
            prog->bind();
            
            geom.draw(Geometry::Primitive_Triangles, 0, 3);
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    
    glfwTerminate();
}