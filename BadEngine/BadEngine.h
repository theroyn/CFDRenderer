#pragma once

#include <string>
#include "Camera.h"
#include "const.h"
#include "FileCopier.h"
#include "OBJParser.h"
#include "Shader.h"
#include "Sphere.h"
#include "Box.h"
#include "Arrow.h"
#include "Line.h"
#include <functional>


struct SimpleBox
{
  SimpleBox() : pos(0.f) {}
  glm::vec3 pos;
};

class BadEngine
{
public:
  BadEngine(std::function<void(int, int, int, int)> logic_key_handler_cb);
  ~BadEngine();

public:
  void init();
  void run();
  void draw();

public:
  bool loop_done() const;
  operator bool() const;

public:
  int get_status() const { return status_; }
  std::string get_message() const { return msg_; }

public:
  Sphere *get_sphere(size_t id) const;
  void set_sphere_radius(float rad) { sphere_rad_ = rad; }
  float get_sphere_radius(float rad) const { return sphere_rad_; }
  void set_sphere_pos(int id, float x, float y, float z);
  void set_sphere_velocity(int id, float x, float y, float z);
  size_t add_sphere(float x, float y, float z, bool renderable);
  void set_world_dims(glm::vec3 dims);
  glm::vec3 get_world_center() const { return cube_.pos; }
  glm::vec3 get_world_dims() const { return cube_scale_; }

  size_t add_box(const glm::vec3 &center, const glm::vec3 &dims);
  Box *get_box(size_t id) const;
  size_t add_line(const glm::vec3 &start, const glm::vec3 &end);
  Line *get_line(size_t id) const;
  size_t add_arrow(const glm::vec3 &pos, const glm::vec3 &dims);
  Arrow *get_arrow(size_t id) const;

private:
  void demo_add_spheres();
  void process_input();
  void init_sphere_program();
  void draw_sphere_program(const glm::mat4 &view_trans, const glm::mat4 &projection_trans);
  void init_boxes_program();
  void draw_boxes_program(const glm::mat4 &view_trans, const glm::mat4 &projection_trans);
  void init_cube_program();
  void draw_cube_program(const glm::mat4 &view_trans, const glm::mat4 &projection_trans);
  void init_lines_program();
  void draw_lines_program(const glm::mat4 &view_trans, const glm::mat4 &projection_trans);
  void init_arrows_program();
  void draw_arrows_program(const glm::mat4 &view_trans, const glm::mat4 &projection_trans);
  glm::mat4 &get_model(GLuint vao, size_t idx);
  glm::vec3 &get_pos(size_t idx);
  glm::vec3 &get_vel(size_t idx);
  Renderable add_renderable();

private:
  static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

private:
  GLFWwindow *window_;
  int status_;
  std::string msg_;
  Camera *cam_;

  OBJParser parser_;

  // (vao id --> vector of model transformations)
  std::unordered_map<GLuint, std::vector<glm::mat4>> models_by_vao_;

  struct State
  {
    State(const glm::vec3 &p, const glm::vec3 &v) : p(p), v(v) {}
    glm::vec3 p;
    glm::vec3 v;
  };
  std::vector<State> states_;

  std::vector<Sphere *> spheres_;
  Shader sphere_shader_programme_;
  GLuint sphere_vao_ = 0;
  size_t sphere_count_ = 0;
  float sphere_rad_;

  std::vector<Box *> boxes_;
  Shader box_shader_programme_;
  GLuint box_vao_ = 0;
  size_t box_count_ = 0;
  std::vector<Line *> lines_;
  Shader line_shader_programme_;
  GLuint line_vao_ = 0;

  std::vector<Arrow *> arrows_;
  GLuint arrow_vao_ = 0;
  size_t arrow_count_ = 0;
  Shader arrow_shader_programme_;

  Shader cube_shader_programme_;
  GLuint cube_vbos_[2];
  GLuint cube_vao_[1];
  glm::vec3 cube_scale_;
  SimpleBox cube_;
  std::function<void(int, int, int, int)> logic_key_handler_cb_;
  const GLuint screen_width_ = 1920;
  const GLuint screen_height_ = 1080;
  FileCopier file_copier_;
};
