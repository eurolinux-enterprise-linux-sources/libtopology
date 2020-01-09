
	if (rc == 0)
		topology_free_context(ctx);
	printf("passed.\n");

	/* Valgrind complains about open fds if we don't do this. */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	return 0;
}
