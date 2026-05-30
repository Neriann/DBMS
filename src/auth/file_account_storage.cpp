#include "auth/file_account_storage.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace auth {
namespace {

constexpr char field_separator = '\t';
constexpr std::string_view wildcard_scope = "*";

std::string escape_field(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\\' || ch == '\t' || ch == '\n' || ch == '\r') {
            escaped.push_back('\\');
        }
        switch (ch) {
        case '\t':
            escaped.push_back('t');
            break;
        case '\n':
            escaped.push_back('n');
            break;
        case '\r':
            escaped.push_back('r');
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string unescape_field(const std::string &value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaping = false;
    for (const char ch : value) {
        if (escaping) {
            switch (ch) {
            case 't':
                unescaped.push_back('\t');
                break;
            case 'n':
                unescaped.push_back('\n');
                break;
            case 'r':
                unescaped.push_back('\r');
                break;
            default:
                unescaped.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        unescaped.push_back(ch);
    }
    if (escaping) {
        unescaped.push_back('\\');
    }
    return unescaped;
}

std::vector<std::string> split_fields(const std::string &line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, field_separator)) {
        fields.push_back(unescape_field(field));
    }
    if (!line.empty() && line.back() == field_separator) {
        fields.emplace_back();
    }
    return fields;
}

void write_fields(std::ostream &out, const std::vector<std::string> &fields) {
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i != 0) {
            out << field_separator;
        }
        out << escape_field(fields[i]);
    }
    out << '\n';
}

void ensure_file(const std::filesystem::path &path) {
    if (std::filesystem::exists(path)) {
        return;
    }
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to create " + path.string());
    }
}

std::vector<User> read_users(const std::filesystem::path &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open " + path.string());
    }

    std::vector<User> users;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 5) {
            throw std::runtime_error("invalid users.tbl row");
        }
        User user;
        user.id = fields[0];
        user.username = fields[1];
        user.password_hash = fields[2];
        user.salt = fields[3];
        user.is_admin = fields[4] == "1" || fields[4] == "true";
        users.push_back(std::move(user));
    }
    return users;
}

void write_users(const std::filesystem::path &path, const std::vector<User> &users) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write " + path.string());
    }
    for (const User &user : users) {
        write_fields(out,
                     {user.id,
                      user.username,
                      user.password_hash,
                      user.salt,
                      user.is_admin ? "1" : "0"});
    }
}

std::string subject_type_to_string(const rbac::SubjectType type) {
    switch (type) {
    case rbac::SubjectType::User:
        return "user";
    case rbac::SubjectType::Group:
        return "group";
    case rbac::SubjectType::Default:
        return "default";
    }
    throw std::runtime_error("unknown subject type");
}

rbac::SubjectType subject_type_from_string(const std::string &value) {
    if (value == "user") {
        return rbac::SubjectType::User;
    }
    if (value == "group") {
        return rbac::SubjectType::Group;
    }
    if (value == "default") {
        return rbac::SubjectType::Default;
    }
    throw std::runtime_error("unknown subject type '" + value + "'");
}

std::string permission_to_string(const rbac::Permission permission) {
    switch (permission) {
    case rbac::Permission::ReadTable:
        return "read_table";
    case rbac::Permission::WriteTable:
        return "write_table";
    case rbac::Permission::CreateTable:
        return "create_table";
    case rbac::Permission::DropTable:
        return "drop_table";
    case rbac::Permission::CreateDatabase:
        return "create_database";
    case rbac::Permission::DropDatabase:
        return "drop_database";
    case rbac::Permission::ManageUsers:
        return "manage_users";
    case rbac::Permission::GrantPermissions:
        return "grant_permissions";
    }
    throw std::runtime_error("unknown permission");
}

rbac::Permission permission_from_string(const std::string &value) {
    if (value == "read_table") return rbac::Permission::ReadTable;
    if (value == "write_table") return rbac::Permission::WriteTable;
    if (value == "create_table") return rbac::Permission::CreateTable;
    if (value == "drop_table") return rbac::Permission::DropTable;
    if (value == "create_database") return rbac::Permission::CreateDatabase;
    if (value == "drop_database") return rbac::Permission::DropDatabase;
    if (value == "manage_users") return rbac::Permission::ManageUsers;
    if (value == "grant_permissions") return rbac::Permission::GrantPermissions;
    throw std::runtime_error("unknown permission '" + value + "'");
}

std::string policy_key(const rbac::Policy &policy) {
    return subject_type_to_string(policy.subject_type) + '\x1f'
        + policy.subject_id + '\x1f'
        + policy.database_name + '\x1f'
        + policy.table_name + '\x1f'
        + permission_to_string(policy.permission);
}

rbac::Policy default_read_policy() {
    rbac::Policy policy;
    policy.subject_type = rbac::SubjectType::Default;
    policy.subject_id = "";
    policy.database_name = std::string(wildcard_scope);
    policy.table_name = std::string(wildcard_scope);
    policy.permission = rbac::Permission::ReadTable;
    policy.allowed = true;
    return policy;
}

} // namespace

FileAccountStorage::FileAccountStorage(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
    std::filesystem::create_directories(data_dir_);
    ensure_file(users_path());
    ensure_file(groups_path());
    ensure_file(user_groups_path());
    ensure_file(permissions_path());

    const auto default_policy = default_read_policy();
    const auto existing = policies();
    const auto default_key = policy_key(default_policy);
    const auto has_default_read = std::ranges::any_of(existing, [&default_key](const rbac::Policy &policy) {
        return policy_key(policy) == default_key;
    });
    if (!has_default_read) {
        save_policy(default_policy);
    }
}

std::optional<User> FileAccountStorage::find_by_username(const std::string &username) const {
    for (const User &user : read_users(users_path())) {
        if (user.username == username) {
            return user;
        }
    }
    return std::nullopt;
}

std::optional<User> FileAccountStorage::find_by_id(const std::string &user_id) const {
    for (const User &user : read_users(users_path())) {
        if (user.id == user_id) {
            return user;
        }
    }
    return std::nullopt;
}

bool FileAccountStorage::has_users() const {
    return !read_users(users_path()).empty();
}

void FileAccountStorage::save_user(const User &user) {
    auto users = read_users(users_path());
    const auto same_id = [&user](const User &candidate) { return candidate.id == user.id; };
    const auto same_username = [&user](const User &candidate) {
        return candidate.username == user.username && candidate.id != user.id;
    };
    if (std::ranges::any_of(users, same_username)) {
        throw std::runtime_error("user already exists");
    }

    const auto it = std::ranges::find_if(users, same_id);
    if (it == users.end()) {
        users.push_back(user);
    } else {
        *it = user;
    }
    write_users(users_path(), users);
}

std::vector<rbac::Group> FileAccountStorage::groups() const {
    std::ifstream in(groups_path());
    if (!in) {
        throw std::runtime_error("failed to open " + groups_path().string());
    }

    std::vector<rbac::Group> result;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 2) {
            throw std::runtime_error("invalid groups.tbl row");
        }
        result.push_back({fields[0], fields[1]});
    }
    return result;
}

std::vector<std::string> FileAccountStorage::group_ids_for_user(const std::string &user_id) const {
    std::ifstream in(user_groups_path());
    if (!in) {
        throw std::runtime_error("failed to open " + user_groups_path().string());
    }

    std::vector<std::string> result;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 2) {
            throw std::runtime_error("invalid user_groups.tbl row");
        }
        if (fields[0] == user_id) {
            result.push_back(fields[1]);
        }
    }
    return result;
}

std::vector<rbac::Policy> FileAccountStorage::policies() const {
    std::ifstream in(permissions_path());
    if (!in) {
        throw std::runtime_error("failed to open " + permissions_path().string());
    }

    std::vector<rbac::Policy> result;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 6) {
            throw std::runtime_error("invalid permissions.tbl row");
        }
        rbac::Policy policy;
        policy.subject_type = subject_type_from_string(fields[0]);
        policy.subject_id = fields[1];
        policy.database_name = fields[2];
        policy.table_name = fields[3];
        policy.permission = permission_from_string(fields[4]);
        policy.allowed = fields[5] == "1" || fields[5] == "true";
        result.push_back(std::move(policy));
    }
    return result;
}

void FileAccountStorage::save_group(const rbac::Group &group) {
    auto existing = groups();
    const auto duplicate_name = [&group](const rbac::Group &candidate) {
        return candidate.name == group.name && candidate.id != group.id;
    };
    if (std::ranges::any_of(existing, duplicate_name)) {
        throw std::runtime_error("group already exists");
    }
    const auto it = std::ranges::find_if(existing, [&group](const rbac::Group &candidate) {
        return candidate.id == group.id;
    });
    if (it == existing.end()) {
        existing.push_back(group);
    } else {
        *it = group;
    }

    std::ofstream out(groups_path(), std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write " + groups_path().string());
    }
    for (const auto &item : existing) {
        write_fields(out, {item.id, item.name});
    }
}

void FileAccountStorage::add_user_to_group(const std::string &user_id, const std::string &group_id) {
    std::ifstream in(user_groups_path());
    if (!in) {
        throw std::runtime_error("failed to open " + user_groups_path().string());
    }

    std::vector<std::pair<std::string, std::string>> links;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() != 2) {
            throw std::runtime_error("invalid user_groups.tbl row");
        }
        links.emplace_back(fields[0], fields[1]);
    }
    if (std::ranges::any_of(links, [&](const auto &link) {
        return link.first == user_id && link.second == group_id;
    })) {
        return;
    }
    links.emplace_back(user_id, group_id);

    std::ofstream out(user_groups_path(), std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write " + user_groups_path().string());
    }
    for (const auto &[stored_user_id, stored_group_id] : links) {
        write_fields(out, {stored_user_id, stored_group_id});
    }
}

void FileAccountStorage::save_policy(const rbac::Policy &policy) {
    auto existing = policies();
    const auto key = policy_key(policy);
    const auto it = std::ranges::find_if(existing, [&key](const rbac::Policy &candidate) {
        return policy_key(candidate) == key;
    });
    if (it == existing.end()) {
        existing.push_back(policy);
    } else {
        *it = policy;
    }

    std::ofstream out(permissions_path(), std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write " + permissions_path().string());
    }
    for (const auto &item : existing) {
        write_fields(out,
                     {subject_type_to_string(item.subject_type),
                      item.subject_id,
                      item.database_name,
                      item.table_name,
                      permission_to_string(item.permission),
                      item.allowed ? "1" : "0"});
    }
}

std::filesystem::path FileAccountStorage::users_path() const {
    return data_dir_ / "users.tbl";
}

std::filesystem::path FileAccountStorage::groups_path() const {
    return data_dir_ / "groups.tbl";
}

std::filesystem::path FileAccountStorage::user_groups_path() const {
    return data_dir_ / "user_groups.tbl";
}

std::filesystem::path FileAccountStorage::permissions_path() const {
    return data_dir_ / "permissions.tbl";
}

} // namespace auth
