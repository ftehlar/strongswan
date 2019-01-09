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
#include <assert.h>
#include "vaa_test.pb-c.h"
#include "../vaa_json.h"

#define INPUT_FILE "input_1"

int main()
{
    char *json;
    long file_size;
    FILE *f = fopen(INPUT_FILE, "rb");
    assert(f && "failed to open input file");

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);

    json = malloc(file_size);
    assert(file_size == fread(json, sizeof(char), file_size, f));
    assert(json);
    json[file_size - 1] = '\0'; /* remove new line */
    Person *p = vaa_to_proto(json, &person__descriptor);

    assert(!strcmp(p->name, "Mr. John Doe"));
    assert(p->has_id && p->id == 42);
    assert(!strcmp(p->email, "john@doe.example"));
    assert(p->position_case == PERSON__POSITION_INTERNAL_EMPLOYERS_ID);
    assert(p->internal_employers_id == 99);
    assert(p->n_phones == 2);
    assert(!strcmp(p->phones[0]->number, "12345"));
    assert(!strcmp(p->phones[1]->number, "56789"));
    assert(!p->phones[0]->has_type);
    assert(p->phones[1]->has_type && p->phones[1]->type == PERSON__PHONE_TYPE__HOME);
    assert(NULL == p->empty_message);

    fclose(f);
    free(json);
    person__free_unpacked(p, 0);
    return EXIT_SUCCESS;
}
