CREATE DATABASE "ClientMonitorService"
    WITH 
    OWNER = postgres
    ENCODING = 'UTF8'
    LC_COLLATE = 'en_US.utf8'
    LC_CTYPE = 'en_US.utf8'
    TABLESPACE = pg_default
    CONNECTION LIMIT = -1;

CREATE SCHEMA public
    AUTHORIZATION postgres;

COMMENT ON SCHEMA public
    IS 'standard public schema';

GRANT ALL ON SCHEMA public TO PUBLIC;

GRANT ALL ON SCHEMA public TO postgres;


CREATE TYPE public."LogSeverity" AS ENUM
    ('fatal', 'error', 'warning', 'info', 'verbose', 'debug', 'aduitSuccess', 'aduitFailure');

ALTER TYPE public."LogSeverity"
    OWNER TO postgres;

CREATE TYPE public."OperationSystem" AS ENUM
    ('Linux', 'Windows', 'OTHERs');

ALTER TYPE public."OperationSystem"
    OWNER TO postgres;

CREATE TABLE public."Client"
(
    "ClientID" integer NOT NULL DEFAULT nextval('"Client_ClientID_seq"'::regclass),
    "ClientName" text COLLATE pg_catalog."default" NOT NULL,
    "ClientOS" "OperationSystem" NOT NULL,
    "ClientOSVersion" text COLLATE pg_catalog."default" NOT NULL,
    "ClientUniqueID" text COLLATE pg_catalog."default" NOT NULL,
    "ClientRegisterTime" timestamp without time zone,
    CONSTRAINT "Client_pkey" PRIMARY KEY ("ClientID"),
    CONSTRAINT "UniqueClientUniqueID" UNIQUE ("ClientUniqueID")
)

WITH (
    autovacuum_enabled = TRUE
)
TABLESPACE pg_default;

ALTER TABLE public."Client"
    OWNER to postgres;


CREATE TABLE public."WindowsEvents"
(
    "EventID" integer NOT NULL DEFAULT nextval('"WindowsEvents_EventID_seq"'::regclass),
    "ClientID" integer NOT NULL DEFAULT nextval('"WindowsEvents_ClientID_seq"'::regclass),
    "EventSeverity" "LogSeverity" NOT NULL,
    "EventTimestamp" timestamp with time zone NOT NULL,
    "EventScope" text COLLATE pg_catalog."default" NOT NULL,
    "EventMessage" text COLLATE pg_catalog."default" NOT NULL,
    CONSTRAINT "WindowsEvents_pkey" PRIMARY KEY ("EventID"),
    CONSTRAINT "FK_ClientID" FOREIGN KEY ("ClientID")
        REFERENCES public."Client" ("ClientID") MATCH SIMPLE
        ON UPDATE NO ACTION
        ON DELETE NO ACTION
        NOT VALID
)

TABLESPACE pg_default;

ALTER TABLE public."WindowsEvents"
    OWNER to postgres;

CREATE TABLE public."WindowsEventsXML"
(
    "EventID" integer NOT NULL DEFAULT nextval('"WindowsEventsXML_EventID_seq"'::regclass),
    "EventXML" text COLLATE pg_catalog."default" NOT NULL,
    CONSTRAINT "WindowsEventsXML_pkey" PRIMARY KEY ("EventID"),
    CONSTRAINT "FKEventID" FOREIGN KEY ("EventID")
        REFERENCES public."WindowsEvents" ("EventID") MATCH SIMPLE
        ON UPDATE NO ACTION
        ON DELETE NO ACTION
)

TABLESPACE pg_default;

ALTER TABLE public."WindowsEventsXML"
    OWNER to postgres;