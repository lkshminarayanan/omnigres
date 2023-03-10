CREATE TABLE routes (
   name text NOT NULL,
   query text NOT NULL,
   priority INT NOT NULL
);

INSERT INTO routes (name, query, priority) VALUES
  ('test', $$SELECT omni_httpd.http_response(body => 'test') FROM request WHERE request.path = '/test'$$, 1),
  ('ping', $$SELECT omni_httpd.http_response(body => 'pong') FROM request WHERE request.path = '/ping'$$, 1);

\pset format wrapped
\pset columns 80

-- Preview the query
SELECT omni_httpd.cascading_query(name, query) FROM routes GROUP BY priority ORDER BY priority DESC;

\pset format aligned

-- Try it
BEGIN;
WITH listener AS (INSERT INTO omni_httpd.listeners (address, port) VALUES ('127.0.0.1', 9100) RETURNING id),
     handler AS (INSERT INTO omni_httpd.handlers (query) SELECT omni_httpd.cascading_query(name, query) FROM routes GROUP BY priority ORDER BY priority DESC RETURNING id)
INSERT INTO omni_httpd.listeners_handlers (listener_id, handler_id)
SELECT listener.id, handler.id
FROM listener, handler;
DELETE FROM omni_httpd.configuration_reloads;
END;

CALL omni_httpd.wait_for_configuration_reloads(1);

\! curl --retry-connrefused --retry 10  --retry-max-time 10 -w '\n\n' --silent http://localhost:9100/test

\! curl --retry-connrefused --retry 10  --retry-max-time 10 -w '\n\n' --silent http://localhost:9100/ping

-- CTE handling

\pset format wrapped
\pset columns 80

SELECT omni_httpd.cascading_query(name, query) FROM (
  VALUES
  ('test', $$WITH test AS (SELECT 1 AS val) SELECT omni_httpd.http_response(body => 'test') FROM request, Test WHERE request.path = '/test' and test.val = 1$$, 1),
  ('ping', $$WITH test AS (SELECT 1 AS val) SELECT omni_httpd.http_response(body => 'pong') FROM request, Test WHERE request.path = '/ping' and test.val = 1$$, 1))
  AS routes(name, query, priority) GROUP BY priority ORDER BY priority DESC;

 \pset format aligned