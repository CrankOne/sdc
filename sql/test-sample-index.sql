-- Example types
--  Follows some names from NA64 experiment, as an example.
INSERT INTO types (id, name) VALUES (1, 'SADCCalib');
INSERT INTO types (id, name) VALUES (2, 'APVCalib');

-- Insert periods
INSERT INTO periods (id, from_key, to_key) VALUES (1, 1000, 1999);
INSERT INTO periods (id, from_key, to_key) VALUES (2, 2000, 2999);

-- Overlapping
INSERT INTO periods (id, from_key, to_key) VALUES (3, 1500, 2500);
INSERT INTO periods (id, from_key, to_key) VALUES (4, 1800, 2200);

-- Insert documents
--  
INSERT INTO documents (
    id, path, mod_time, size, hashsum, parse_error, content,
    default_type_id, default_period_id
) VALUES (
    1, '/data/calib1.dat', 1620000000, 2048, 'abc123def456', 0, NULL,
    1, -- default document type -- SADCCalib
    1  -- default document validity, 1000-1999
);

INSERT INTO documents (
    id, path, mod_time, size, hashsum, parse_error, content,
    default_type_id, default_period_id
) VALUES (
    2, '/data/calib2.dat', 1625000000, 4096, '789ghi101jkl', 0, NULL,
    2, -- default document type -- APVCalib
    3  -- default document validity, 1500-2500
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

-- insert a few blocks to reference types and periods (if blocks table is involved)
INSERT INTO blocks (id, doc_id, line_start, n_lines, type_id, period_id)
VALUES (1,
    1,  -- in doc #1
    1,  -- line start
    50, -- of n lines
    1,  -- of type SADCCalib
    1   -- valid in 1st period (1000-1999)
);

INSERT INTO blocks (id, doc_id, line_start, n_lines, type_id, period_id)
VALUES (2,
    2,  -- in doc #2
    1,  -- line start
    50, -- of n lines
    2,  -- of type APVCalib
    4   -- valid in 3rd period (1800-2200)
);

