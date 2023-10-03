## WAP project 1: Iterate Properties

This project provides an `iterate` library that provides a function generating names
of properties in a given object and its prototype chain. The yielded property names
may be optionally limited based on their property descriptors.

### Usage

The `iterate` module exports a single function: `iterateProperties`. It takes two
parameters: the target object and, optionally, the filtering object with the shape
of a property descriptor.

If a filter is supplied, its `configurable`, `enumerable`, `value` and `writable`
properties are matched with the values in the target object's properties' descriptors.

### Implementation notes

The specification doesn't say how properties identified by Symbols should be returned.
As it asks for _property names_, this implementation calls `toString()` on these
keys, thus it always returns strings and symbols are returned as `"Symbol(id)"`.

The order in which the properties are returned is **not specified** by the assignment.
In this implementation, they are yielded in order they appear in the result of `Reflect.ownKeys`.
First, properties found in the provided source object are returned, then properties
from its prototype, etc.

### Tests

Several unit tests for the library are included. The tests use [jasmine](https://jasmine.github.io/index.html)
framework. They may be run separately using `npm test` or together with the performance
tests (see below) using `test.sh`. The tests' code includes custom matchers to work with
iterables.

### Performance

Out of curiosity, I tried several approaches to the problem. First, the algorithm can 
be implemented either using recursion, or using a while cycle. Second, an object's 
properties can be retrieved using [various functions](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Enumerability_and_ownership_of_properties#querying_object_properties).
Third, the generator may either return values one-by-one using `yield` or accumulate
the results in an array and return them all using `yield*`. Fourth, _functional-like_
approach may be used to filter `(string | Symbol)[]` array and map it to an array
of strings, or it can be done _imperatively_ while iterating.

The `iterate_alternative` module contains different implementations.

The table below shows their running times of a single run on i7-1165G7, node.js v16.13.1
when running for a chain of 1,000 objects with 10,000 properties each, with no filters
used. The results show that, unsurprisingly, the recursive implementations run several 
orders of magnitude slower than their iterative counterparts. The actual method of listing 
an object's properties doesn't change the result significantly, except for the one that
uses `Object.getOwnPropertyDescriptors`.


| Implementation             | Used methods                                                                          | Time  |
|----------------------------|---------------------------------------------------------------------------------------|-------|
| recursive_fetchDescriptors | Object.getOwnPropertyDescriptors,<br>Reflect.ownKeys                                  | 150 s |
| recursive_functional       | Object.getOwnPropertyDescriptor,<br>Object.getPrototypeOf                             | 138 s |
| recursive_reflectOnInput   | Object.getOwnPropertyDescriptor,<br>Object.getPrototypeOf                             | 135 s |
| iterative_fetchDescriptors | Object.getOwnPropertyDescriptors,<br>Reflect.ownKeys                                  | 10 s  |
| iterative_functional       | Object.getOwnPropertyDescriptor,<br>Object.getPrototypeOf                             | 2.3 s |
| iterative_reflectOnInput   | Object.getOwnPropertyDescriptor,<br>Object.getPrototypeOf                             | 2.3 s |
| iterative_objectMethods    | Object.getOwnPropertyNames,<br>Object.getOwnPropertySymbols,<br>Object.getPrototypeOf | 2.4 s |