#include "Simulator.h"

#include <stdlib.h> /* srand, rand */
#include <time.h>   /* time */
#include <glm/gtx/norm.hpp>
#include <functional>
#include <utils.h>

static const glm::vec3 GRAVITY(0.f, -.9f, 0.f);

Simulator::Simulator(unsigned int spheres_n) : sphere_coll_alg_(sphere_coll_alg::grid),
                                               base_h_(.03f),
                                               damping_(.09f),
                                               spheres_n_(spheres_n),
                                               sphere_rad_(.1f),
                                               engine_(std::bind(&Simulator::key_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)),
                                               impulse_solver_(this)
{
  col_solver_ = SolverFactory::create(sphere_coll_alg_, sphere_rad_);

  // Create the world settings
  reactphysics3d::PhysicsWorld::WorldSettings settings;
  settings.gravity = reactphysics3d::Vector3(GRAVITY.x, GRAVITY.y, GRAVITY.z);

  // Create the physics world with your settings
  world_ = physics_common_.createPhysicsWorld(settings);
}

void Simulator::handle_collisions()
{
  // Sphere collisions
  col_solver_->handle_collisions(spheres_);

  // Box collisions
  for (Box *box : boxes_)
  {
    box->color = glm::vec3(1., .2, .11);
  }

  int solver_iteration_counter = 0;

  do
  {
    if (solver_iteration_counter == 30)
    {
      break;
    }

    impulse_solver_.clear();

    world_->testCollision(impulse_solver_);

    if (impulse_solver_.has_contacts())
    {
      impulse_solver_.solve();
      solver_iteration_counter++;
    }
  } while (impulse_solver_.had_collisions());

  if (solver_iteration_counter > 0)
  {
    std::cout << "Solved after " << solver_iteration_counter << " iterations\n";
  }
}

Simulator::~Simulator()
{
  for (auto &body_pair : bodies_)
  {
    reactphysics3d::CollisionBody *body = body_pair.second;
    world_->destroyCollisionBody(body);
  }
  physics_common_.destroyPhysicsWorld(world_);
}

void Simulator::add_global_force(const std::string &name, glm::vec3 f)
{
  // override existing force if there is one
  g_forces_[name] = f;
}
void Simulator::add_global_torque(const std::string &name, glm::vec3 f)
{
  // override existing force if there is one
  g_torques_[name] = f;
}

void Simulator::remove_global_force(const std::string &name)
{
  auto it = g_forces_.find(name);
  if (it == g_forces_.end())
  {
    std::cerr << "No force named " << name << "\n";
  }
  else
  {
    g_forces_.erase(it);
  }
}

void Simulator::remove_global_torque(const std::string &name)
{
  auto it = g_torques_.find(name);
  if (it == g_torques_.end())
  {
    std::cerr << "No torque named " << name << "\n";
  }
  else
  {
    g_torques_.erase(it);
  }
}

void Simulator::integrate()
{
  static bool init = true;

  double curr_time = glfwGetTime();

  if (init)
  {
    init = false;
    last_time_ = curr_time;
  }

  float delta = (float)(curr_time - last_time_);
  last_time_ = curr_time;

  float h = base_h_ * 133.33f * delta;
  integrate_spheres(h);
  integrate_boxes(h);
}

static float get_rand(float low = -.5f, float high = .5f)
{
  float r3 = low + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (high - low)));

  return r3;
}

void Simulator::integrate_spheres(float h)
{
  for (Sphere *sphere : spheres_)
  {
    glm::vec3 acc(0.f);

    for (const auto &f : g_forces_)
    {
      acc += f.second / sphere->mass;
    }

    // internal forces calculations
    glm::vec3 damping_force = -damping_ * sphere->vel;
    acc += damping_force / sphere->mass;
    static Sphere *lowSphere = nullptr;
    static size_t cnt = 0;

    if (!lowSphere && sphere->pos.y < -2.5f)
    {
      lowSphere = sphere;
    }
    if (lowSphere == sphere && cnt++ % 100 == 0)
    {
      std::cout << "h:" << h << ", prev pos (" << lowSphere->pos.x << "," << lowSphere->pos.y << "," << lowSphere->pos.z << ")\n";
    }
    // semi-implicit euler integration
    sphere->vel += h * acc;
    sphere->pos += h * sphere->vel;
    if (lowSphere == sphere && cnt % 100 == 0)
    {
      std::cout << "curr pos (" << lowSphere->pos.x << "," << lowSphere->pos.y << "," << lowSphere->pos.z << ")\n\n";
    }

    sphere->colliders_.clear();
  }
}

void Simulator::integrate_boxes(float h)
{
  for (auto box : boxes_)
  {
    glm::vec3 acc(0.f);
    glm::vec3 torque(0.f);

    for (auto f : g_forces_)
    {
      acc += f.second * box->inv_mass;
    }
    for (auto f : g_torques_)
    {
      torque += f.second * box->inv_mass;
    }

    // internal forces calculations
    glm::vec3 damping_force = -damping_ * box->vel;
    acc += damping_force * box->inv_mass;

    float angular_damping = 1.f / (1.f + damping_);

    // DUDU use semi-implicit euler
    box->center += h * box->vel;

    // linear momentum

    glm::vec3 P_dot(0.f);
    if (box->inv_mass > .0001)
      P_dot = acc / box->inv_mass;

    box->P += h * P_dot;
    box->vel = box->P * box->inv_mass;

    glm::mat3 R = glm::toMat3(box->orientation);
    box->IInv = R * box->IBodyInv * glm::transpose(R);

    // angular_momentum
    glm::vec3 L_dot = torque;
    box->L += L_dot * h * angular_damping;
    box->angular_vel = box->IInv * box->L;

    box->orientation += 0.5f * glm::quat(0.f, box->angular_vel) * box->orientation * h;
    box->orientation = glm::normalize(box->orientation);
  }

  // update reactphysics3d world
  for (auto &body_pair : bodies_)
  {
    reactphysics3d::CollisionBody *body = body_pair.second;
    Box *box = reinterpret_cast<Box *>(body->getUserData());
    reactphysics3d::Vector3 pos(box->center.x, box->center.y, box->center.z);
    reactphysics3d::Quaternion orientation(box->orientation.w, reactphysics3d::Vector3(box->orientation.x, box->orientation.y, box->orientation.z));
    reactphysics3d::Transform transform(pos, orientation);

    body->setTransform(transform);
  }
}

void Simulator::init()
{
  engine_.set_sphere_radius(sphere_rad_);
  engine_.set_world_dims(col_solver_->dims());
  engine_.init();

  add_global_force("gravity", GRAVITY);

  std::vector<size_t> elem_indices;
  const glm::vec3 dims = engine_.get_world_dims();
  float w = dims.x / 3.f;
  float h = dims.y / 3.f;
  float d = dims.z / 3.f;
  const bool small_start = false;

  // add spheres
  for (unsigned int i = 0; i < spheres_n_; ++i)
  {
    if (small_start)
      elem_indices.push_back(engine_.add_sphere(get_rand(), get_rand(), get_rand()));
    else
      elem_indices.push_back(engine_.add_sphere(get_rand(-w, w), get_rand(-h, h), get_rand(-d, d)));
  }

  for (size_t ind : elem_indices)
  {
    Sphere *s = engine_.get_sphere(ind);
    s->vel.x = get_rand(-.2f, .2f);
    s->vel.y = get_rand(-.2f, .2f);
    s->vel.z = get_rand(-.2f, .2f);
    spheres_.push_back(s);
  }

  // add boxes
  elem_indices.clear();
  for (unsigned int i = 0; i < 50; ++i)
  {
    elem_indices.push_back(engine_.add_box(glm::vec3(get_rand(-w, w), get_rand(-h, h), get_rand(-d, d)), glm::vec3(.4f, .4f, .4f)));
  }

  for (size_t ind : elem_indices)
  {
    Box *b = engine_.get_box(ind);
    b->set_initial_vel(glm::vec3(get_rand(-.5f, .5f), get_rand(-.5f, .5f), get_rand(-.9f, .9f)));
    boxes_.push_back(b);
  }

  // add boundaries
  const glm::vec3 center = engine_.get_world_center();
  Box *floor = new Box(center + glm::vec3(0.f, -dims.y, 0.f), dims, true);
  Box *ceiling = new Box(center + glm::vec3(0.f, dims.y, 0.f), dims, true);
  Box *back = new Box(center + glm::vec3(0.f, 0.f, -dims.z), dims, true);
  Box *front = new Box(center + glm::vec3(0.f, 0.f, dims.z), dims, true);
  Box *right = new Box(center + glm::vec3(dims.x, 0.f, 0.f), dims, true);
  Box *left = new Box(center + glm::vec3(-dims.x, 0.f, 0.f), dims, true);

  boxes_.push_back(floor);
  boxes_.push_back(ceiling);
  boxes_.push_back(back);
  boxes_.push_back(front);
  boxes_.push_back(right);
  boxes_.push_back(left);

  for (size_t i = 0; i < boxes_.size(); ++i)
  {
    Box *box = boxes_[i];
    reactphysics3d::Vector3 pos(box->center.x, box->center.y, box->center.z);
    reactphysics3d::Quaternion orientation(box->orientation.w, reactphysics3d::Vector3(box->orientation.x, box->orientation.y, box->orientation.z));
    reactphysics3d::Transform transform(pos, orientation);

    reactphysics3d::CollisionBody *body = world_->createCollisionBody(transform);
    body->setUserData(box);
    bodies_.emplace(i, body);
    const reactphysics3d::Vector3 halfExtents(box->dims.x * .5f, box->dims.y * .5f, box->dims.z * .5f);

    reactphysics3d::BoxShape *shape = physics_common_.createBoxShape(halfExtents);
    reactphysics3d::Collider *collider = body->addCollider(shape, reactphysics3d::Transform::identity());
  }

  debug_line_ = engine_.get_line(engine_.add_line(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 0.f)));
}

static void print_fps()
{
  namespace cr = std::chrono;
  static size_t frame_cnt = 0;
  frame_cnt++;
  static auto last_print = cr::system_clock::now().time_since_epoch();
  static auto previous = cr::system_clock::now().time_since_epoch();
  auto current = cr::system_clock::now().time_since_epoch();
  cr::microseconds diff_from_last_print = cr::duration_cast<cr::microseconds>(current - last_print);
  if (diff_from_last_print >= cr::seconds(1))
  {
    last_print = current;

    cr::microseconds diff_from_last_frame = cr::duration_cast<cr::microseconds>(current - previous);
    std::cout << "frame num:" << frame_cnt << ", Time to process last frame (milliseconds): " << diff_from_last_frame.count() / 1000.0
              << " FPS: " << 1.0 / ((double)diff_from_last_frame.count() / 1000'000.0) << "\n";
  }
  previous = current;
}

void Simulator::run()
{
  while (!engine_.loop_done())
  {
    engine_.draw();

    handle_collisions();

    integrate();
    print_fps();
  }
}

void Simulator::key_callback(int key, int scancode, int action, int mods)
{
  switch (key)
  {
  case GLFW_KEY_P:
  {
    if (action == GLFW_PRESS)
    {
      std::cout << "P press!!!\n";

      glm::vec3 force(get_rand(-1.f, 1.f), .5f, get_rand(-1.f, 1.f));
      std::cout << "adding:(" << force.x << "," << force.y << "," << force.z << "\n";
      add_global_force("P-force", force);
    }
    else if (action == GLFW_RELEASE)
    {
      std::cout << "P release!!!\n";
      remove_global_force("P-force");
    }
    else if (action == GLFW_REPEAT)
    {
      // ignored
    }
    else
    {
      std::cout << "P unknown action:" << action << "\n";
    }
  }
  break;
  case GLFW_KEY_T:
  {
    if (action == GLFW_PRESS)
    {
      std::cout << "T press!!!\n";

      glm::vec3 torque(get_rand(-.1f, .1f), .5f, get_rand(-.1f, .1f));
      std::cout << "adding:(" << torque.x << "," << torque.y << "," << torque.z << "\n";
      add_global_torque("torque", torque);
    }
    else if (action == GLFW_RELEASE)
    {
      std::cout << "T release!!!\n";
      remove_global_torque("torque");
    }
    else if (action == GLFW_REPEAT)
    {
      // ignored
    }
    else
    {
      std::cout << "P unknown action:" << action << "\n";
    }
  }
  break;
  case GLFW_KEY_R:
  {
    if (action == GLFW_PRESS)
    {
      std::cout << "R press!!!\n";
      boxes_[0]->P.z += .3f;
    }
  }
  break;
  case GLFW_KEY_E:
  {
    if (action == GLFW_PRESS)
    {
      std::cout << "R press!!!\n";
      boxes_[0]->P.z -= .3f;
    }
  }
  break;
  default:
  {
    // ignore
  }
  }
}
