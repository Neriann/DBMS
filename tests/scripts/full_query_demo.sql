CREATE DATABASE query_demo_full;
USE query_demo_full;

CREATE TABLE users (
    id INT INDEXED,
    name STRING NOT NULL DEFAULT "anon",
    age INT DEFAULT 18,
    city STRING DEFAULT "Moscow",
    active INT DEFAULT 1,
    note STRING DEFAULT NULL
);

INSERT INTO users (id, name, age, city, act, note) VALUE
    (1, "Ann", 20, "Moscow", 1, "admin"),
    (2, "Bob", 25, "Berlin", 1, NULL),
    (3, "Alice", 30, "Amsterdam", 0, "guest"),
    (4, "Kate", NULL, "Kazan", 1, NULL);

INSERT INTO users (id, name) VALUE
    (5, "DefaultUser");

SELECT * FROM users;

SELECT id AS user_id, name AS user_name, city FROM users;

SELECT name, age FROM users WHERE age == NULL;
SELECT name, age FROM users WHERE age != NULL;

UPDATE users SET age = 40 WHERE id == 4;

SELECT name, age FROM users WHERE age >= 20;
SELECT id, name, age FROM users WHERE age BETWEEN 20 AND 30;

SELECT id, name FROM users WHERE name LIKE "A.*";

SELECT id, name, active FROM users WHERE active == 1 AND age >= 20;
SELECT id, name, active FROM users WHERE (id == 1 OR id == 3) AND name != "Bob";

UPDATE users SET city = "Paris" WHERE id == 2;
SELECT * FROM users WHERE id == 2;

CREATE TABLE numbers (
    id INT INDEXED,
    value INT DEFAULT 0
);

INSERT INTO numbers (id, value) VALUE
    (1, -10),
    (2, -5),
    (3, 0),
    (4, 5),
    (5, 10);

SELECT * FROM numbers WHERE value < 0;
UPDATE numbers SET value = -value WHERE id == 1;
SELECT * FROM numbers;

DELETE FROM users WHERE id == 4;
SELECT * FROM users;

CREATE TABLE products (
    sku STRING INDEXED,
    title STRING NOT NULL,
    price INT DEFAULT 0
);

INSERT INTO products (sku, title, price) VALUE
    ("A001", "Keyboard", 100),
    ("B010", "Mouse", 50),
    ("C100", "Monitor", 300),
    ("D200", "Laptop", 1000);

SELECT * FROM products WHERE sku == "B010";
SELECT * FROM products WHERE sku BETWEEN "B000" AND "D000";

SELECT COUNT(*) AS total_users FROM users;
SELECT COUNT(age) AS users_with_age, SUM(age) AS total_age, AVG(age) AS average_age FROM users WHERE active == 1;

SELECT name, city FROM query_demo_full.users WHERE id >= 1 AND id <= 3;

CREATE TABLE temporal_log (
    id INT INDEXED,
    message STRING DEFAULT "created"
);

INSERT INTO temporal_log (id, message) VALUE
    (1, "first"),
    (2, "second");

UPDATE temporal_log SET message = "updated" WHERE id == 1;
SELECT * FROM temporal_log;

REVERT temporal_log 1970.01.01-00:00:00.000;
SELECT * FROM temporal_log;

CREATE TABLE scratch (
    id INT INDEXED,
    value STRING DEFAULT "tmp"
);

INSERT INTO scratch (id) VALUE (1);
SELECT * FROM scratch;
DROP TABLE scratch;

CREATE DATABASE query_demo_to_drop;
DROP DATABASE query_demo_to_drop;
