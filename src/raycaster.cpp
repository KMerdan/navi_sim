// Copyright (c) 2021 OUXT Polaris
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <navi_sim/raycaster.hpp>

#include <quaternion_operation/quaternion_operation.h>

#include <unordered_map>
#include <string>
#include <algorithm>
#include <vector>
#include <utility>
#include <iostream>

namespace navi_sim
{
Raycaster::Raycaster()
: primitive_ptrs_(0),
  device_(nullptr),
  scene_(nullptr)
{
  device_ = rtcNewDevice(nullptr);
}

Raycaster::Raycaster(std::string embree_config)
: primitive_ptrs_(0),
  device_(nullptr),
  scene_(nullptr)
{
  device_ = rtcNewDevice(embree_config.c_str());
}

Raycaster::~Raycaster()
{
  rtcReleaseDevice(device_);
  std::cout << dumpPrimitives() << std::endl;
}

nlohmann::json Raycaster::dumpPrimitives() const
{
  nlohmann::json j;
  for (auto && pair : primitive_ptrs_) {
    j[pair.first] = pair.second->toJson();
  }
  return j;
}

const sensor_msgs::msg::PointCloud2 Raycaster::raycast(
  geometry_msgs::msg::Pose origin,
  double horizontal_resolution,
  std::vector<double> vertical_angles,
  double max_distance, double min_distance
)
{
  std::vector<geometry_msgs::msg::Quaternion> directions;
  double horizontal_angle = 0;
  while (horizontal_angle <= (2 * M_PI)) {
    horizontal_angle = horizontal_angle + horizontal_resolution;
    for (const auto vertical_angle : vertical_angles) {
      geometry_msgs::msg::Vector3 rpy;
      rpy.x = 0;
      rpy.y = vertical_angle;
      rpy.z = horizontal_angle;
      auto quat = quaternion_operation::convertEulerAngleToQuaternion(rpy);
      directions.emplace_back(quat);
    }
  }
  return raycast(origin, directions, max_distance, min_distance);
}

const sensor_msgs::msg::PointCloud2 Raycaster::raycast(
  geometry_msgs::msg::Pose origin,
  std::vector<geometry_msgs::msg::Quaternion> directions,
  double max_distance, double min_distance)
{
  scene_ = rtcNewScene(device_);
  pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
  for (auto && pair : primitive_ptrs_) {
    pair.second->addToScene(device_, scene_);
  }
  rtcCommitScene(scene_);
  RTCIntersectContext context;
  rtcInitIntersectContext(&context);
  RTCRayHit rayhit;
  for (auto direction : directions) {
    rayhit.ray.org_x = origin.position.x;
    rayhit.ray.org_y = origin.position.y;
    rayhit.ray.org_z = origin.position.z;
    rayhit.ray.tfar = max_distance;
    rayhit.ray.tnear = min_distance;
    rayhit.ray.flags = false;
    const auto ray_direction = origin.orientation * direction;
    const auto rotation_mat = quaternion_operation::getRotationMatrix(ray_direction);
    const auto rotated_direction = rotation_mat * Eigen::Vector3d(1.0f, 0.0f, 0.0f);
    rayhit.ray.dir_x = rotated_direction[0];
    rayhit.ray.dir_y = rotated_direction[1];
    rayhit.ray.dir_z = rotated_direction[2];
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rtcIntersect1(scene_, &context, &rayhit);

    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
      double distance = rayhit.ray.tfar;
      const auto vector = quaternion_operation::getRotationMatrix(direction) * Eigen::Vector3d(
        1.0f,
        0.0f,
        0.0f) *
        distance;
      pcl::PointXYZI p;
      p.x = vector[0];
      p.y = vector[1];
      p.z = vector[2];
      cloud->emplace_back(p);
    }
  }
  sensor_msgs::msg::PointCloud2 pointcloud_msg;
  pcl::toROSMsg(*cloud, pointcloud_msg);
  rtcReleaseScene(scene_);
  return pointcloud_msg;
}
}  // namespace navi_sim
