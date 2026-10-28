/*
 * IBAN: PostgreSQL functions and datatype
 * Copyright © 2016-2026 Yorick de Wid <yorick17@outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *********************************************************************/

extern "C"
{

#include "postgres.h"

#if PG_VERSION_NUM >= 160000
#include "varatt.h"
#endif

#include "common/hashfn.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
}

#include <climits>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <regex>
#include <stdexcept>
#include <string>

namespace
{

	/* ISO 13616: an IBAN is at most 34 characters long. */
	constexpr size_t IBAN_MAX_LENGTH = 34;

	/*
	 * Longest input that is even looked at. The print format has a space every
	 * four characters (42 characters for 34 significant ones); anything longer
	 * cannot be an IBAN and is rejected without scanning it.
	 */
	constexpr size_t IBAN_MAX_INPUT = 64;

	/* Longest part of an invalid value that is repeated in an error message. */
	constexpr int IBAN_ERROR_ECHO = 40;

	struct CountrySpec
	{
		const char *code;
		size_t length;
		const char *structure;
	};

	/*
	 * IBAN length and BBAN structure per country. A structure is composed of
	 * blocks of 3 characters (one letter and 2 digits). Each block represents a
	 * logical group in the typical representation of the BBAN. For each group,
	 * the letter indicates which characters are allowed in this group and the
	 * following 2-digits number tells the length of the group.
	 */
	const CountrySpec countries[] = {
		{"AD", 24, "F04F04A12"},
		{"AE", 23, "F03F16"},
		{"AL", 28, "F08A16"},
		{"AT", 20, "F05F11"},
		{"AZ", 28, "U04A20"},
		{"BA", 20, "F03F03F08F02"},
		{"BE", 16, "F03F07F02"},
		{"BG", 22, "U04F04F02A08"},
		{"BH", 22, "U04A14"},
		{"BR", 29, "F08F05F10U01A01"},
		{"CH", 21, "F05A12"},
		{"CR", 21, "F03F14"},
		{"CY", 28, "F03F05A16"},
		{"CZ", 24, "F04F06F10"},
		{"DE", 22, "F08F10"},
		{"DK", 18, "F04F09F01"},
		{"DO", 28, "U04F20"},
		{"EE", 20, "F02F02F11F01"},
		{"ES", 24, "F04F04F01F01F10"},
		{"FI", 18, "F06F07F01"},
		{"FO", 18, "F04F09F01"},
		{"FR", 27, "F05F05A11F02"},
		{"GB", 22, "U04F06F08"},
		{"GE", 22, "U02F16"},
		{"GI", 23, "U04A15"},
		{"GL", 18, "F04F09F01"},
		{"GR", 27, "F03F04A16"},
		{"GT", 28, "A04A20"},
		{"HR", 21, "F07F10"},
		{"HU", 28, "F03F04F01F15F01"},
		{"IE", 22, "U04F06F08"},
		{"IL", 23, "F03F03F13"},
		{"IS", 26, "F04F02F06F10"},
		{"IT", 27, "U01F05F05A12"},
		{"KW", 30, "U04A22"},
		{"KZ", 20, "F03A13"},
		{"LB", 28, "F04A20"},
		{"LC", 32, "U04F24"},
		{"LI", 21, "F05A12"},
		{"LT", 20, "F05F11"},
		{"LU", 20, "F03A13"},
		{"LV", 21, "U04A13"},
		{"MA", 28, "F03F05F14F02"},
		{"MC", 27, "F05F05A11F02"},
		{"MD", 24, "U02A18"},
		{"ME", 22, "F03F13F02"},
		{"MK", 19, "F03A10F02"},
		{"MR", 27, "F05F05F11F02"},
		{"MT", 31, "U04F05A18"},
		{"MU", 30, "U04F02F02F12F03U03"},
		{"NL", 18, "U04F10"},
		{"NO", 15, "F04F06F01"},
		{"PK", 24, "U04A16"},
		{"PL", 28, "F08F16"},
		{"PS", 29, "U04A21"},
		{"PT", 25, "F04F04F11F02"},
		{"RO", 24, "U04A16"},
		{"RS", 22, "F03F13F02"},
		{"SA", 24, "F02A18"},
		{"SE", 24, "F03F16F01"},
		{"SI", 19, "F05F08F02"},
		{"SK", 24, "F04F06F10"},
		{"SM", 27, "U01F05F05A12"},
		{"ST", 25, "F08F11F02"},
		{"TL", 23, "F03F14F02"},
		{"TN", 24, "F02F03F13F02"},
		{"TR", 26, "F05F01A16"},
		{"VG", 24, "U04F16"},
		{"XK", 20, "F04F10F02"},
		{"AO", 25, "F21"},
		{"BF", 27, "F23"},
		{"BI", 16, "F12"},
		{"BJ", 28, "F24"},
		{"CI", 28, "U01F23"},
		{"CM", 27, "F23"},
		{"CV", 25, "F21"},
		{"DZ", 24, "F20"},
		{"IR", 26, "F22"},
		{"JO", 30, "A04F22"},
		{"MG", 27, "F23"},
		{"ML", 28, "U01F23"},
		{"MZ", 25, "F21"},
		{"QA", 29, "U04A21"},
		{"SN", 28, "U01F23"},
		{"UA", 29, "F25"},
		{"EG", 27, "F23"},
		{"CG", 27, "F23"},
		{"GA", 27, "F23"},
	};

	inline bool isDigit(char c) noexcept
	{
		return c >= '0' && c <= '9';
	}

	inline bool isUpper(char c) noexcept
	{
		return c >= 'A' && c <= 'Z';
	}

	/* ASCII only, so the result does not depend on the database locale. */
	inline char toUpper(char c) noexcept
	{
		return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
	}

	/**
	 * Canonical form: ASCII spaces removed (the print format has them every four
	 * characters), ASCII letters upper-cased. The result is NUL-terminated.
	 *
	 * @param out must hold IBAN_MAX_LENGTH + 1 bytes
	 * @returns the canonical length, or -1 when the input is too long to be an IBAN
	 */
	int normalize(const char *data, size_t length, char *out) noexcept
	{
		if (length > IBAN_MAX_INPUT)
			return -1;

		size_t n = 0;
		for (size_t i = 0; i < length; i++)
		{
			if (data[i] == ' ')
				continue;
			if (n == IBAN_MAX_LENGTH)
				return -1;
			out[n++] = toUpper(data[i]);
		}
		out[n] = '\0';
		return static_cast<int>(n);
	}

	/**
	 * Calculates the MOD 97 10 of the passed IBAN as specified in ISO7064.
	 * The first four characters are moved to the end and every letter is
	 * replaced by two digits (A = 10, ..., Z = 35).
	 *
	 * @returns the remainder, or -1 if the IBAN contains other characters
	 */
	int iso7064Mod97_10(const char *iban, size_t length) noexcept
	{
		int remainder = 0;

		for (size_t n = 0; n < length; n++)
		{
			char c = iban[(n + 4) % length];

			if (isDigit(c))
				remainder = (remainder * 10 + (c - '0')) % 97;
			else if (isUpper(c))
				remainder = (remainder * 100 + (c - 'A' + 10)) % 97;
			else
				return -1;
		}

		return remainder;
	}

	/**
	 * Parse the IBAN structure used to configure each iban specification and
	 * returns a matching regular expression.
	 *
	 * @param {string} structure the structure to parse
	 * @returns {string}
	 */
	std::string parseStructure(const char *structure)
	{
		size_t length = std::strlen(structure);
		std::string result;

		if (length == 0 || length % 3 != 0)
			throw std::invalid_argument("malformed BBAN structure");

		for (size_t i = 0; i < length; i += 3)
		{
			const char *format;

			/* Parse each structure block (1-char + 2-digits) */
			switch (structure[i])
			{
			case 'A':
				format = "0-9A-Z";
				break;
			case 'B':
				format = "0-9A-Z";
				break;
			case 'C':
				format = "A-Z";
				break;
			case 'F':
				format = "0-9";
				break;
			case 'L':
				format = "A-Z";
				break;
			case 'U':
				format = "A-Z";
				break;
			case 'W':
				format = "0-9A-Z";
				break;
			default:
				throw std::invalid_argument("malformed BBAN structure");
			}

			if (!isDigit(structure[i + 1]) || !isDigit(structure[i + 2]))
				throw std::invalid_argument("malformed BBAN structure");

			int repeats = (structure[i + 1] - '0') * 10 + (structure[i + 2] - '0');

			result += "[";
			result += format;
			result += "]{" + std::to_string(repeats) + "}";
		}

		return result;
	}

	class Validate
	{
	public:
		Validate()
		{
			for (const auto &country : countries)
				specifications.emplace(country.code, Specification{country.length, country.structure, nullptr});
		}

		bool isValid(const char *data, size_t length);

	private:
		struct Specification
		{
			size_t length;
			const char *structure;
			/* Compiled on first use, then kept for the life of the backend */
			std::unique_ptr<std::regex> bban;
		};

		std::map<std::string, Specification> specifications;
	};

	bool Validate::isValid(const char *data, size_t length)
	{
		/* Reject anything too small or too large before doing any work */
		if (length < 4 || length > IBAN_MAX_LENGTH)
			return false;

		/* Convert uppercase */
		char account[IBAN_MAX_LENGTH];
		for (size_t i = 0; i < length; i++)
			account[i] = toUpper(data[i]);

		/* Check digits are always numeric (ISO 13616) */
		if (!isDigit(account[2]) || !isDigit(account[3]))
			return false;

		/* Match on country */
		auto found = specifications.find(std::string(account, 2));
		if (found == specifications.end())
			return false;

		Specification &spec = found->second;
		if (spec.length != length)
			return false;

		if (!spec.bban)
			spec.bban.reset(new std::regex(parseStructure(spec.structure),
										   std::regex::ECMAScript | std::regex::optimize));

		/* Test accountnumber */
		return std::regex_match(account + 4, account + length, *spec.bban) && iso7064Mod97_10(account, length) == 1;
	}

	/*
	 * Created on first use instead of at load time, so a failure while
	 * building it is reported as an error instead of terminating the backend.
	 */
	Validate &validator()
	{
		static Validate instance;
		return instance;
	}

	/**
	 * Separate CXX and C logic. No C++ exception may escape into PostgreSQL,
	 * and ereport() must not longjmp over live C++ objects, so the exception
	 * is turned into a plain message first and reported after the handler.
	 *
	 * @returns {bool}
	 */
	bool account_validate(const char *iban, size_t length)
	{
		int sqlstate;
		char message[256];

		try
		{
			return validator().isValid(iban, length);
		}
		catch (const std::bad_alloc &)
		{
			sqlstate = ERRCODE_OUT_OF_MEMORY;
			strlcpy(message, "out of memory", sizeof(message));
		}
		catch (const std::exception &e)
		{
			sqlstate = ERRCODE_INTERNAL_ERROR;
			strlcpy(message, e.what(), sizeof(message));
		}
		catch (...)
		{
			sqlstate = ERRCODE_INTERNAL_ERROR;
			strlcpy(message, "unknown exception", sizeof(message));
		}

		ereport(ERROR,
				(errcode(sqlstate),
				 errmsg("iban validation failed: %s", message)));
		return false; /* keep compiler quiet */
	}

	/**
	 * Normalize and validate in one go.
	 *
	 * @param out receives the canonical value, must hold IBAN_MAX_LENGTH + 1 bytes
	 * @returns {bool} true when the input is a valid IBAN
	 */
	bool account_canonical(const char *input, size_t length, char *out, size_t *outLength)
	{
		int n = normalize(input, length, out);
		if (n < 0)
			return false;
		*outLength = static_cast<size_t>(n);
		return account_validate(out, *outLength);
	}

	/**
	 * Report an invalid value. Only the start of the value is repeated, so a
	 * huge input cannot produce a huge error message; the clip never splits a
	 * multibyte character.
	 */
	[[noreturn]] void report_invalid(int sqlstate, const char *value, size_t length)
	{
		int clip = pg_mbcliplen(value, static_cast<int>(length < INT_MAX ? length : INT_MAX), IBAN_ERROR_ECHO);

		if (static_cast<size_t>(clip) < length)
			ereport(ERROR,
					(errcode(sqlstate),
					 errmsg("invalid iban format for value: \"%.*s...\" (%zu bytes)",
							clip, value, length)));

		ereport(ERROR,
				(errcode(sqlstate),
				 errmsg("invalid iban format for value: \"%.*s\"", clip, value)));
	}

	/* Byte-wise comparison, IBANs are plain ASCII and not collatable. */
	int iban_cmp_internal(text *a, text *b)
	{
		int alen = VARSIZE_ANY_EXHDR(a);
		int blen = VARSIZE_ANY_EXHDR(b);
		int result = memcmp(VARDATA_ANY(a), VARDATA_ANY(b), Min(alen, blen));

		if (result != 0)
			return result < 0 ? -1 : 1;
		return (alen > blen) - (alen < blen);
	}

} // namespace

extern "C"
{

#if PG_VERSION_NUM >= 180000
	PG_MODULE_MAGIC_EXT(.name = "iban", .version = "1.2");
#else
	PG_MODULE_MAGIC;
#endif

	/**************************************************************************
	 * Input/Output functions
	 **************************************************************************/

	PG_FUNCTION_INFO_V1(ibanin);

	Datum
	ibanin(PG_FUNCTION_ARGS)
	{
		char *inputText = PG_GETARG_CSTRING(0);
		size_t inputLength = strlen(inputText);
		char canonical[IBAN_MAX_LENGTH + 1];
		size_t canonicalLength = 0;

		if (!account_canonical(inputText, inputLength, canonical, &canonicalLength))
			report_invalid(ERRCODE_INVALID_TEXT_REPRESENTATION, inputText, inputLength);

		PG_RETURN_TEXT_P(cstring_to_text_with_len(canonical, canonicalLength));
	}

	/* Convert type output */

	PG_FUNCTION_INFO_V1(ibanout);

	Datum
	ibanout(PG_FUNCTION_ARGS)
	{
		PG_RETURN_CSTRING(TextDatumGetCString(PG_GETARG_DATUM(0)));
	}

	/**************************************************************************
	 * Binary Input/Output functions
	 **************************************************************************/

	PG_FUNCTION_INFO_V1(ibanrecv);

	Datum
	ibanrecv(PG_FUNCTION_ARGS)
	{
		StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
		text *result;
		char *str;
		int nbytes;
		char canonical[IBAN_MAX_LENGTH + 1];
		size_t canonicalLength = 0;

		str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);

		/* Binary input must not bypass the validation (and normalisation) done by ibanin */
		if (!account_canonical(str, nbytes, canonical, &canonicalLength))
			report_invalid(ERRCODE_INVALID_BINARY_REPRESENTATION, str, nbytes);

		result = cstring_to_text_with_len(canonical, canonicalLength);
		pfree(str);
		PG_RETURN_TEXT_P(result);
	}

	PG_FUNCTION_INFO_V1(ibansend);

	Datum
	ibansend(PG_FUNCTION_ARGS)
	{
		text *t = PG_GETARG_TEXT_PP(0);
		StringInfoData buf;

		pq_begintypsend(&buf);
		pq_sendtext(&buf, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
		PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
	}

	/* Manually verify a text */

	PG_FUNCTION_INFO_V1(iban_validate);

	Datum
	iban_validate(PG_FUNCTION_ARGS)
	{
		/* Never detoast more than the longest input that is looked at, plus one byte so that longer values are still rejected */
		text *iban = PG_GETARG_TEXT_P_SLICE(0, 0, IBAN_MAX_INPUT + 1);

		char canonical[IBAN_MAX_LENGTH + 1];
		size_t canonicalLength = 0;

		PG_RETURN_BOOL(account_canonical(VARDATA_ANY(iban), VARSIZE_ANY_EXHDR(iban), canonical, &canonicalLength));
	}

	/**************************************************************************
	 * Comparison functions (btree and hash operator classes)
	 **************************************************************************/

#define IBAN_COMPARE(name, op)                       \
	PG_FUNCTION_INFO_V1(name);                       \
                                                     \
	Datum                                            \
	name(PG_FUNCTION_ARGS)                           \
	{                                                \
		text *a = PG_GETARG_TEXT_PP(0);              \
		text *b = PG_GETARG_TEXT_PP(1);              \
		int result = iban_cmp_internal(a, b);        \
                                                     \
		PG_FREE_IF_COPY(a, 0);                       \
		PG_FREE_IF_COPY(b, 1);                       \
		PG_RETURN_BOOL(result op 0);                 \
	}

	IBAN_COMPARE(iban_eq, ==)
	IBAN_COMPARE(iban_ne, !=)
	IBAN_COMPARE(iban_lt, <)
	IBAN_COMPARE(iban_le, <=)
	IBAN_COMPARE(iban_gt, >)
	IBAN_COMPARE(iban_ge, >=)

	PG_FUNCTION_INFO_V1(iban_cmp);

	Datum
	iban_cmp(PG_FUNCTION_ARGS)
	{
		text *a = PG_GETARG_TEXT_PP(0);
		text *b = PG_GETARG_TEXT_PP(1);
		int result = iban_cmp_internal(a, b);

		PG_FREE_IF_COPY(a, 0);
		PG_FREE_IF_COPY(b, 1);
		PG_RETURN_INT32(result);
	}

	PG_FUNCTION_INFO_V1(iban_hash);

	Datum
	iban_hash(PG_FUNCTION_ARGS)
	{
		text *t = PG_GETARG_TEXT_PP(0);
		Datum result = hash_any((unsigned char *)VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));

		PG_FREE_IF_COPY(t, 0);
		return result;
	}

	PG_FUNCTION_INFO_V1(iban_hash_extended);

	Datum
	iban_hash_extended(PG_FUNCTION_ARGS)
	{
		text *t = PG_GETARG_TEXT_PP(0);
		Datum result = hash_any_extended((unsigned char *)VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t),
										 PG_GETARG_INT64(1));

		PG_FREE_IF_COPY(t, 0);
		return result;
	}

} // extern "C"
