-- Insert types
INSERT INTO types (id, name) VALUES (1, 'SADCCalib');
INSERT INTO types (id, name) VALUES (2, 'APVCalib');

-- Insert periods
-- Non-overlapping
INSERT INTO periods (id, from_key, to_key) VALUES (1, 1000, 1999);
INSERT INTO periods (id, from_key, to_key) VALUES (2, 2000, 2999);

-- Overlapping
INSERT INTO periods (id, from_key, to_key) VALUES (3, 1500, 2500);
INSERT INTO periods (id, from_key, to_key) VALUES (4, 1800, 2200);

-- Insert documents
INSERT INTO documents (
    id, path, mod_time, size, hashsum, parse_error, content,
    default_type_id, default_period_id
) VALUES (
    1, '/data/calib1.json', 1620000000, 2048, 'abc123def456', 0, NULL, 1, 1
);

INSERT INTO documents (
    id, path, mod_time, size, hashsum, parse_error, content,
    default_type_id, default_period_id
) VALUES (
    2, '/data/calib2.json', 1625000000, 4096, '789ghi101jkl', 0, NULL, 2, 3
);

-- Insert metadata for document 1
INSERT INTO metadata (id, doc_id, line, key, value)
VALUES (1, 1, 10, 'author', 'John Doe');

INSERT INTO metadata (id, doc_id, line, key, value)
VALUES (2, 1, 20, 'version', '1.0');

-- Insert metadata for document 2
INSERT INTO metadata (id, doc_id, line, key, value)
VALUES (3, 2, 15, 'author', 'Alice');

INSERT INTO metadata (id, doc_id, line, key, value)
VALUES (4, 2, 30, 'comments', 'Test data set');

-- Optionally, insert a few blocks to reference types and periods (if blocks table is involved)
INSERT INTO blocks (id, doc_id, line_start, line_end, type_id, period_id)
VALUES (1, 1, 1, 50, 1, 1);

INSERT INTO blocks (id, doc_id, line_start, line_end, type_id, period_id)
VALUES (2, 2, 51, 100, 2, 3);

