#include "camera.hpp"

#include "glm/gtc/matrix_transform.hpp"

Camera::Camera()
{
    setView(vec3(), quat());
    setProjection(1.f, glm::radians(45.f), 1.f, 1000.f);
}
    
void Camera::setView(const vec3& position_, const quat& orientation_)
{
    position = position_;
    orientation = orientation_;
    
    view = glm::lookAt(position, position + orientation * vec3(1, 0, 0), orientation * vec3(0, 0, 1));
    viewProjection = projection * view;
}

void Camera::setProjection(float aspect_, float fov_, float znear_, float zfar_)
{
    assert(aspect_ > 0);
    assert(fov_ > 0 && fov_ < glm::pi);
    assert(znear_ < zfar_);
    
    aspect = aspect_;
    fov = fov_;
    znear = znear_;
    zfar = zfar_;
    
    projection = glm::perspectiveFov(fov, aspect, 1.f, znear, zfar);
    viewProjection = projection * view;
}