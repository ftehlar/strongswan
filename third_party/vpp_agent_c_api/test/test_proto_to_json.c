/*
 * Copyright (c) 2019 Cisco and/or its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vaa_test.pb-c.h"
#include "../vaa_json.h"

#define EXPECTED_RESULT_FILE "expected_1"

int str_eq(char *expected, char *actual)
{
    size_t i;
    size_t len1 = strlen(expected);
    size_t len2 = strlen(actual);

    assert(len1 == len2);

    for (i = 0; i < len1; i++) {
        if (expected[i] != actual[i]) {
            fprintf(stderr, "string differs at %lu actual: %c, expected %c !\n",
                    i, actual[i], expected[i]);
            return 0;
        }
    }
    return 1;
}

int main()
{
    char *json;
    long file_size;
    FILE *f = fopen(EXPECTED_RESULT_FILE, "rb");
    assert(f && "failed to open input file");

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);

    json = malloc(file_size);
    assert(file_size == fread(json, sizeof(char), file_size, f));
    assert(json);
    json[file_size - 1] = '\0'; /* remove new line */

    Person p = PERSON__INIT;
    p.name = "Mr. John Doe";
    p.email = "john@doe.example";

    Person__PhoneNumber *phones[2];
    Person__PhoneNumber phone1 = PERSON__PHONE_NUMBER__INIT;
    Person__PhoneNumber phone2 = PERSON__PHONE_NUMBER__INIT;

    phone1.number = "12345";
    phone1.type = PERSON__PHONE_TYPE__WORK;
    phone1.has_type = 1;

    phone2.number = "56789";

    phones[0] = &phone1;
    phones[1] = &phone2;
    p.phones = phones;
    p.n_phones = 2;

    p.position_case = PERSON__POSITION_EXTERNAL_WORKERS_LOCATION;
    p.external_workers_location = "Barcelona";

    char *s = vaa_to_json((void *)&p);
    assert(s);
    fprintf(stderr, "'%s'", s);
    assert(str_eq(s, json));

    free(s);
    free(json);
    fclose(f);
    return EXIT_SUCCESS;
}
