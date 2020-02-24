#include <navi_sim/navi_sim_component.h>

namespace navi_sim
{
    NaviSimComponent::NaviSimComponent(const rclcpp::NodeOptions & options)
    : Node("navi_sim", options),
        broadcaster_(this)
    {
        using namespace std::chrono_literals;
        update_position_timer_ = this->create_wall_timer(10ms, std::bind(&NaviSimComponent::updatePose, this));
        current_pose_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>("current_pose",1);
        current_twist_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("current_twist",1);
        initial_pose_sub_ = this->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>
            ("initialpose", 1, std::bind(&NaviSimComponent::initialPoseCallback, this, std::placeholders::_1));
        target_twist_sub_ = this->create_subscription<geometry_msgs::msg::Twist>
            ("target_twist", 1, std::bind(&NaviSimComponent::targetTwistCallback, this, std::placeholders::_1));
    }

    void NaviSimComponent::updatePose()
    {
        mtx_.lock();
        // Update Current Pose
        using namespace quaternion_operation;
        geometry_msgs::msg::Vector3 angular_trans_vec;
        angular_trans_vec.z = current_twist_.angular.z * 0.01;
        geometry_msgs::msg::Quaternion angular_trans_quat = convertEulerAngleToQuaternion(angular_trans_vec);
        current_pose_.orientation = quaternion_operation::rotation(current_pose_.orientation,angular_trans_quat);
        Eigen::Vector3d trans_vec;
        trans_vec(0) = current_twist_.linear.x * 0.01;
        trans_vec(1) = current_twist_.linear.y * 0.01;
        Eigen::Matrix3d rotation_mat = quaternion_operation::getRotationMatrix(current_pose_.orientation);
        trans_vec = rotation_mat*trans_vec;
        current_pose_.position.x = trans_vec(0) + current_pose_.position.x;
        current_pose_.position.y = trans_vec(1) + current_pose_.position.y;

        // Publish tf
        geometry_msgs::msg::TransformStamped transform_stamped;
        transform_stamped.header.stamp = get_clock()->now();
        transform_stamped.header.frame_id = "map";
        transform_stamped.child_frame_id = "base_link";
        transform_stamped.transform.translation.x = current_pose_.position.x;
        transform_stamped.transform.translation.y = current_pose_.position.y;
        transform_stamped.transform.translation.z = 0.0;
        transform_stamped.transform.rotation = current_pose_.orientation;
        broadcaster_.sendTransform(transform_stamped);

        // Publish current Pose
        geometry_msgs::msg::PoseStamped current_pose_msg;
        current_pose_msg.pose = current_pose_;
        current_pose_msg.header.stamp = transform_stamped.header.stamp;
        current_pose_msg.header.frame_id = "map";
        current_pose_pub_->publish(current_pose_msg);
        current_twist_pub_->publish(current_twist_);
        mtx_.unlock();
    }

    void NaviSimComponent::targetTwistCallback(const geometry_msgs::msg::Twist::SharedPtr data)
    {
        mtx_.lock();
        current_twist_ = *data;
        mtx_.unlock();
    }

    void NaviSimComponent::initialPoseCallback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr data)
    {
        mtx_.lock();
        current_pose_ = data->pose.pose;
        current_twist_ = geometry_msgs::msg::Twist();
        mtx_.unlock();
    }
}

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(navi_sim::NaviSimComponent)