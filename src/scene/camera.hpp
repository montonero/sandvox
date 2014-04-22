#pragma once

class Camera
{
public:
    Camera();
    
    void setView(const vec3& position, const quat& orientation);
    void setProjection(float aspect, float fov, float znear, float zfar);
    
    const vec3& getPosition() const { return position; }
    const quat& getOrientation() const { return orientation; }
    
    float getAspect() const { return aspect; }
    float getFOV() const { return fov; }
    
    float getZNear() const { return znear; }
    float getZFar() const { return zfar; }
    
    const mat4& getViewMatrix() const { return view; }
    const mat4& getProjectionMatrix() const { return projection; }
    const mat4& getViewProjectionMatrix() const { return viewProjection; }
    
private:
    vec3 position;
    quat orientation;
    
    float aspect;
    float fov;
    
    float znear;
    float zfar;
    
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
};