/* contrib/hocotext/hocotext--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION hocotext" to load this file. \quit

-- PostgreSQL code for HOCOTEXT

CREATE TYPE hocotext;

-- Input and output functions.

CREATE FUNCTION hocotextin(cstring)
RETURNS hocotext
AS 'textin'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotextout(hocotext)
RETURNS cstring
AS 'textout'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotextrecv(internal)
RETURNS hocotext
AS 'textrecv'
LANGUAGE internal STABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotextsend(hocotext)
RETURNS bytea
AS 'textsend'
LANGUAGE internal STABLE STRICT PARALLEL SAFE;

-- create type itself

CREATE TYPE hocotext (
    INPUT          = hocotextin,
    OUTPUT         = hocotextout,
    RECEIVE        = hocotextrecv,
    SEND           = hocotextsend,
    INTERNALLENGTH = VARIABLE,
    STORAGE        = extended,
    -- make it a non-preferred member of string type category
    CATEGORY       = 'S',  -- String
    PREFERRED      = false,
    COLLATABLE     = true
);


-- Type casting functions for those situations where the I/O casts don't
-- automatically kick in.


CREATE FUNCTION hocotext(bpchar)
RETURNS hocotext
AS 'rtrim1'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext(boolean)
RETURNS hocotext
AS 'booltext'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext(inet)
RETURNS hocotext
AS 'network_show'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;



--  Implicit and assignment type casts.

CREATE CAST (hocotext AS text)    WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (hocotext AS varchar) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (hocotext AS bpchar)  WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (text AS hocotext)    WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (varchar AS hocotext) WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (bpchar AS hocotext)  WITH FUNCTION hocotext(bpchar)  AS ASSIGNMENT;
CREATE CAST (boolean AS hocotext) WITH FUNCTION hocotext(boolean) AS ASSIGNMENT;
CREATE CAST (inet AS hocotext)    WITH FUNCTION hocotext(inet)    AS ASSIGNMENT;


-- Operator Functions

CREATE FUNCTION hocotext_eq( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_nq( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_lt( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_le( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_gt( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_ge( hocotext, hocotext )
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE FUNCTION hocotext_extract( hocotext, int4 , int4 )
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION text_extract( hocotext, int4 , int4 )
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_insert( hocotext, int4 , text )
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_delete( hocotext, int4 , int4 )
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- Operators

CREATE OPERATOR = (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    COMMUTATOR = =,
    NEGATOR    = <>,
    PROCEDURE  = hocotext_eq,
    RESTRICT   = eqsel,
    JOIN       = eqjoinsel,
    HASHES,
    MERGES
);

CREATE OPERATOR <> (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    NEGATOR    = =,
    COMMUTATOR = <>,
    PROCEDURE  = hocotext_nq,
    RESTRICT   = neqsel,
    JOIN       = neqjoinsel
);

CREATE OPERATOR < (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    NEGATOR    = >=,
    COMMUTATOR = >,
    PROCEDURE  = hocotext_lt,
    RESTRICT   = scalarltsel,
    JOIN       = scalarltjoinsel
);

CREATE OPERATOR <= (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    NEGATOR    = >,
    COMMUTATOR = >=,
    PROCEDURE  = hocotext_le,
    RESTRICT   = scalarltsel,
    JOIN       = scalarltjoinsel
);

CREATE OPERATOR >= (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    NEGATOR    = <,
    COMMUTATOR = <=,
    PROCEDURE  = hocotext_ge,
    RESTRICT   = scalargtsel,
    JOIN       = scalargtjoinsel
);

CREATE OPERATOR > (
    LEFTARG    = HOCOTEXT,
    RIGHTARG   = HOCOTEXT,
    NEGATOR    = <=,
    COMMUTATOR = <,
    PROCEDURE  = hocotext_gt,
    RESTRICT   = scalargtsel,
    JOIN       = scalargtjoinsel
);


-- Support functions for indexing 
CREATE FUNCTION hocotext_cmp(hocotext, hocotext)
RETURNS int4
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT IMMUTABLE PARALLEL SAFE;

-- The btree indexing operator class.
CREATE OPERATOR CLASS hocotext_ops
DEFAULT FOR TYPE HOCOTEXT USING btree AS
    OPERATOR    1   <  (hocotext, hocotext),
    OPERATOR    2   <= (hocotext, hocotext),
    OPERATOR    3   =  (hocotext, hocotext),
    OPERATOR    4   >= (hocotext, hocotext),
    OPERATOR    5   >  (hocotext, hocotext),
    FUNCTION    1   hocotext_cmp(hocotext, hocotext);

-- [TODO] hash indexing 


-- Affregates

CREATE FUNCTION hocotext_smaller(hocotext, hocotext)
RETURNS hocotext
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION hocotext_larger(hocotext, hocotext)
RETURNS hocotext
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE AGGREGATE min(hocotext)  (
    SFUNC = hocotext_smaller,
    STYPE = hocotext,
    SORTOP = <,
    PARALLEL = SAFE,
    COMBINEFUNC = hocotext_smaller
);

CREATE AGGREGATE max(hocotext)  (
    SFUNC = hocotext_larger,
    STYPE = hocotext,
    SORTOP = >,
    PARALLEL = SAFE,
    COMBINEFUNC = hocotext_larger
);

-- [TO BE CHECK CORRECTNESS] pattern matching

CREATE FUNCTION textlike(hocotext, hocotext)
RETURNS bool AS 'textlike'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textnlike(hocotext, hocotext)
RETURNS bool AS 'textnlike'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textregexeq(hocotext, hocotext)
RETURNS bool AS 'textregexeq'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textregexne(hocotext, hocotext)
RETURNS bool AS 'textregexne'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR ~ (
    PROCEDURE = textregexeq,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = !~,
    RESTRICT  = regexeqsel,
    JOIN      = regexeqjoinsel
);

CREATE OPERATOR ~* (
    PROCEDURE = textregexeq,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = !~*,
    RESTRICT  = regexeqsel,
    JOIN      = regexeqjoinsel
);

CREATE OPERATOR !~ (
    PROCEDURE = textregexne,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = ~,
    RESTRICT  = regexnesel,
    JOIN      = regexnejoinsel
);

CREATE OPERATOR !~* (
    PROCEDURE = textregexne,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = ~*,
    RESTRICT  = regexnesel,
    JOIN      = regexnejoinsel
);

CREATE OPERATOR ~~ (
    PROCEDURE = textlike,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = !~~,
    RESTRICT  = likesel,
    JOIN      = likejoinsel
);

CREATE OPERATOR ~~* (
    PROCEDURE = textlike,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = !~~*,
    RESTRICT  = likesel,
    JOIN      = likejoinsel
);

CREATE OPERATOR !~~ (
    PROCEDURE = textnlike,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = ~~,
    RESTRICT  = nlikesel,
    JOIN      = nlikejoinsel
);

CREATE OPERATOR !~~* (
    PROCEDURE = textnlike,
    LEFTARG   = hocotext,
    RIGHTARG  = hocotext,
    NEGATOR   = ~~*,
    RESTRICT  = nlikesel,
    JOIN      = nlikejoinsel
);

--
-- Matching hocotext to text.
--

CREATE FUNCTION textlike(hocotext, text)
RETURNS bool AS 'textlike'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textnlike(hocotext, text)
RETURNS bool AS 'textnlike'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textregexeq(hocotext, text)
RETURNS bool AS 'textregexeq'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION textregexne(hocotext, text)
RETURNS bool AS 'textregexne'
LANGUAGE internal IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR ~ (
    PROCEDURE = textregexeq,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = !~,
    RESTRICT  = regexeqsel,
    JOIN      = regexeqjoinsel
);

CREATE OPERATOR ~* (
    PROCEDURE = textregexeq,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = !~*,
    RESTRICT  = regexeqsel,
    JOIN      = regexeqjoinsel
);

CREATE OPERATOR !~ (
    PROCEDURE = textregexne,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = ~,
    RESTRICT  = regexnesel,
    JOIN      = regexnejoinsel
);

CREATE OPERATOR !~* (
    PROCEDURE = textregexne,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = ~*,
    RESTRICT  = regexnesel,
    JOIN      = regexnejoinsel
);

CREATE OPERATOR ~~ (
    PROCEDURE = textlike,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = !~~,
    RESTRICT  = likesel,
    JOIN      = likejoinsel
);

CREATE OPERATOR ~~* (
    PROCEDURE = textlike,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = !~~*,
    RESTRICT  = likesel,
    JOIN      = likejoinsel
);

CREATE OPERATOR !~~ (
    PROCEDURE = textnlike,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = ~~,
    RESTRICT  = nlikesel,
    JOIN      = nlikejoinsel
);

CREATE OPERATOR !~~* (
    PROCEDURE = textnlike,
    LEFTARG   = hocotext,
    RIGHTARG  = text,
    NEGATOR   = ~~*,
    RESTRICT  = nlikesel,
    JOIN      = nlikejoinsel
);