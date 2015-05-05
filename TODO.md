# TODO list

1. Investigate how to get libconfuse to ignore unknown parameters. This is a big issue
   for being able to create firmware updates that are runnable with old versions of fwup.

2. Implement on_error handling

3. Validate that all resources in a task are run. If not - error.

4. Signed updates

5. Non-"verify on the fly" update support

6. Regression tests!

7. Implement require-fwup-version, but this should be done after #1 so that it's more useful.

8. Implement progress
