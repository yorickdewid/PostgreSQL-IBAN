--- IBAN datatype and verificator                            -*- sql -*-
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

CREATE TYPE iban;

CREATE OR REPLACE FUNCTION ibanin(cstring)
  RETURNS iban
  AS 'iban'
  LANGUAGE C
  IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION ibanout(iban)
  RETURNS cstring
  AS 'iban'
  LANGUAGE C
  IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION ibanrecv(internal)
  RETURNS iban
  AS 'iban'
  LANGUAGE C
  IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION ibansend(iban)
  RETURNS bytea
  AS 'iban'
  LANGUAGE C
  IMMUTABLE STRICT;

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

CREATE OR REPLACE FUNCTION iban_validate(text)
  RETURNS boolean AS 'MODULE_PATHNAME'
  LANGUAGE C
  IMMUTABLE STRICT;
COMMENT ON FUNCTION iban_validate (text)
  IS 'Validate IBAN account';
