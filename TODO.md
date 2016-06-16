# TODO list

2. Implement on_error handling
3. Validate that all resources in a task are run. If not - error.
4. Signed updates. UPDATE: A beta attempt at signing is available. I haven't
   validated it in production yet.
5. Non-"verify on the fly" update support
6. Implement require-fwup-version, but this should be done after #1 so that it's more useful.
7. Port to Windows
8. Improve FAT filesystem progress handling. Right now the progress is not
   indicative of what's happening. It could be once the FAT caching code gets
   integrated.
9. Make buffering configurable
