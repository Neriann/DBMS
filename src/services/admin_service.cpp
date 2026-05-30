#include "services/admin_service.hpp"

#include <nlohmann/json.hpp>
#include <random>

namespace services {
namespace {

std::string generate_id() {
    std::random_device device;
    std::mt19937_64 engine(device());
    std::uniform_int_distribution<std::uint64_t> dist;
    return std::to_string(dist(engine));
}

rbac::Policy make_policy(const rbac::SubjectType subject_type,
                         const std::string &subject_id,
                         const std::string &database_name,
                         const std::string &table_name,
                         const rbac::Permission permission,
                         const bool allowed) {
    rbac::Policy policy;
    policy.subject_type = subject_type;
    policy.subject_id = subject_id;
    policy.database_name = database_name;
    policy.table_name = table_name;
    policy.permission = permission;
    policy.allowed = allowed;
    return policy;
}

} // namespace

AdminService::AdminService(rbac::AccessControlStorage &storage)
    : storage_(storage) {
}

std::string AdminService::create_group(const std::string &name) {
    rbac::Group group;
    group.id = generate_id();
    group.name = name;
    storage_.save_group(group);
    return nlohmann::json{{"status", "ok"}, {"id", group.id}, {"name", group.name}}.dump();
}

std::string AdminService::add_user_to_group(const std::string &user_id, const std::string &group_id) {
    storage_.add_user_to_group(user_id, group_id);
    return nlohmann::json{{"status", "ok"}}.dump();
}

std::string AdminService::grant_permission(const rbac::SubjectType subject_type,
                                           const std::string &subject_id,
                                           const std::string &database_name,
                                           const std::string &table_name,
                                           const rbac::Permission permission) {
    storage_.save_policy(make_policy(subject_type, subject_id, database_name, table_name, permission, true));
    return nlohmann::json{{"status", "ok"}}.dump();
}

std::string AdminService::revoke_permission(const rbac::SubjectType subject_type,
                                            const std::string &subject_id,
                                            const std::string &database_name,
                                            const std::string &table_name,
                                            const rbac::Permission permission) {
    storage_.save_policy(make_policy(subject_type, subject_id, database_name, table_name, permission, false));
    return nlohmann::json{{"status", "ok"}}.dump();
}

} // namespace services
