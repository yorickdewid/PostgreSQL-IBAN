--- IBAN datatype and verificator, upgrade from 1.1 to 1.2      -*- sql -*-
---
--- Copyright © 2016-2020 Yorick de Wid <yorick17 at outlook dot com>
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

\echo Use "ALTER EXTENSION iban UPDATE TO '1.2'" to load this file. \quit

ALTER FUNCTION ibanin(cstring) IMMUTABLE STRICT PARALLEL SAFE;
ALTER FUNCTION ibanout(iban) IMMUTABLE STRICT PARALLEL SAFE;
-- STABLE like textrecv/textsend: the result depends on client_encoding
ALTER FUNCTION ibanrecv(internal) STABLE STRICT PARALLEL SAFE;
ALTER FUNCTION ibansend(iban) STABLE STRICT PARALLEL SAFE;
ALTER FUNCTION iban_validate(text) IMMUTABLE STRICT PARALLEL SAFE;

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
