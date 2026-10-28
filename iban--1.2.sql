--- IBAN datatype and verificator                            -*- sql -*-
---
--- Copyright © 2016-2026 Yorick de Wid <yorickdewid@users.noreply.github.com>
---
--- This program is free software: you can redistribute it and/or modify
--- it under the terms of the GNU General Public License as published by
--- the Free Software Foundation, either version 2 of the License, or
--- (at your option) any later version.
---
--- This program is distributed in the hope that it will be useful, but
--- WITHOUT ANY WARRANTY; without even the implied warranty of
--- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
--- General Public License for more details.
---
--- You should have received a copy of the GNU General Public License
--- along with this program.  If not, see
--- <http://www.gnu.org/licenses/>.

\echo Use "CREATE EXTENSION iban" to load this file. \quit

CREATE TYPE iban;

CREATE FUNCTION ibanin(cstring)
  RETURNS iban
  AS 'MODULE_PATHNAME'
  LANGUAGE C
  IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION ibanout(iban)
  RETURNS cstring
  AS 'MODULE_PATHNAME'
  LANGUAGE C
  IMMUTABLE STRICT PARALLEL SAFE;

-- STABLE like textrecv/textsend: the result depends on client_encoding
CREATE FUNCTION ibanrecv(internal)
  RETURNS iban
  AS 'MODULE_PATHNAME'
  LANGUAGE C
  STABLE STRICT PARALLEL SAFE;

CREATE FUNCTION ibansend(iban)
  RETURNS bytea
  AS 'MODULE_PATHNAME'
  LANGUAGE C
  STABLE STRICT PARALLEL SAFE;

CREATE TYPE iban (
    LIKE           = text,
    INPUT          = ibanin,
    OUTPUT         = ibanout,
    RECEIVE        = ibanrecv,
    SEND           = ibansend,
    -- make it a non-preferred member of string type category
    CATEGORY       = 'S',
    PREFERRED      = false
);

COMMENT ON TYPE iban IS 'International Bank Account Number';

CREATE CAST (iban AS text)    WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (iban AS varchar) WITHOUT FUNCTION AS IMPLICIT;
CREATE CAST (iban AS bpchar)  WITHOUT FUNCTION AS ASSIGNMENT;

CREATE FUNCTION iban_validate(text)
  RETURNS boolean AS 'MODULE_PATHNAME'
  LANGUAGE C
  IMMUTABLE STRICT PARALLEL SAFE;
COMMENT ON FUNCTION iban_validate (text)
  IS 'Validate IBAN account';

--
-- Comparison operators and the default btree and hash operator classes
--

CREATE FUNCTION iban_eq(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_ne(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_lt(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_le(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_gt(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_ge(iban, iban) RETURNS boolean
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_cmp(iban, iban) RETURNS integer
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_hash(iban) RETURNS integer
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION iban_hash_extended(iban, bigint) RETURNS bigint
  AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE OPERATOR = (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_eq,
  COMMUTATOR = =, NEGATOR = <>,
  RESTRICT = eqsel, JOIN = eqjoinsel, HASHES, MERGES
);
CREATE OPERATOR <> (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_ne,
  COMMUTATOR = <>, NEGATOR = =,
  RESTRICT = neqsel, JOIN = neqjoinsel
);
CREATE OPERATOR < (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_lt,
  COMMUTATOR = >, NEGATOR = >=,
  RESTRICT = scalarltsel, JOIN = scalarltjoinsel
);
CREATE OPERATOR <= (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_le,
  COMMUTATOR = >=, NEGATOR = >,
  RESTRICT = scalarlesel, JOIN = scalarlejoinsel
);
CREATE OPERATOR > (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_gt,
  COMMUTATOR = <, NEGATOR = <=,
  RESTRICT = scalargtsel, JOIN = scalargtjoinsel
);
CREATE OPERATOR >= (
  LEFTARG = iban, RIGHTARG = iban, PROCEDURE = iban_ge,
  COMMUTATOR = <=, NEGATOR = <,
  RESTRICT = scalargesel, JOIN = scalargejoinsel
);

CREATE OPERATOR CLASS iban_ops
  DEFAULT FOR TYPE iban USING btree AS
    OPERATOR 1 <,
    OPERATOR 2 <=,
    OPERATOR 3 =,
    OPERATOR 4 >=,
    OPERATOR 5 >,
    FUNCTION 1 iban_cmp(iban, iban);

CREATE OPERATOR CLASS iban_ops
  DEFAULT FOR TYPE iban USING hash AS
    OPERATOR 1 =,
    FUNCTION 1 iban_hash(iban),
    FUNCTION 2 iban_hash_extended(iban, bigint);
