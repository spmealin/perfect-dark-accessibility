/* Frozen pre-refactor adapter policy from 07b091ec5, for differential tests
 * only. This is not linked into the game or an alternate runtime path. */
static void referenceCaneApplyDropValidation(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer,
		f32 *barrierdistance, s32 *result)
{
	struct accessibilitycanepathdiagnostic *diag = &sample->pathdiagnostic;
	struct accessibilitycanepathresult *path = &diag->path;
	f32 distance;
	if (sample->evaluation.cue != ACCESSIBILITY_CANE_CUE_DROP) return;
	sample->legacydropdistance = sample->terraindistance;
	sample->legacydropheight = sample->terrainheight;
	sample->dropvalidation = accessibilityCaneValidateDrop(path,
			sample->legacydropdistance, sample->radius
					* ACCESSIBILITY_CANE_PATH_TERRAIN_MIN_RUNWAY_RADII);
	if (sample->dropvalidation == ACCESSIBILITY_CANE_DROP_FALLBACK) return;
	distance = path->cuedistance;
	if (sample->dropvalidation == ACCESSIBILITY_CANE_DROP_CONFIRMED_EDGE) {
		sample->terraindistance = distance;
		sample->audiosource.x = observer->origin.x + sample->direction.x * distance;
		sample->audiosource.y = observer->camera.y;
		sample->audiosource.z = observer->origin.z + sample->direction.z * distance;
		sample->evaluation.observation.terraindistance = distance;
	} else if (sample->dropvalidation == ACCESSIBILITY_CANE_DROP_BARRIER_FIRST) {
		distance = path->stopdistance;
		sample->drop = false;
		sample->terrain = 0;
		*barrierdistance = distance;
		*result = CDRESULT_COLLISION;
		sample->result = CDRESULT_COLLISION;
		sample->obstacle = (struct prop *)diag->blocker;
		sample->obstacletype = sample->obstacle ? sample->obstacle->type : -1;
		sample->rawhit.x = observer->origin.x + sample->direction.x * distance;
		sample->rawhit.y = observer->camera.y;
		sample->rawhit.z = observer->origin.z + sample->direction.z * distance;
		sample->evaluation.observation.barrier = ACCESSIBILITY_CANE_BLOCKED;
		sample->evaluation.observation.barrierdistance = distance;
		sample->evaluation.cue = ACCESSIBILITY_CANE_CUE_BARRIER;
		sample->evaluation.source = ACCESSIBILITY_CANE_SOURCE_BARRIER;
	} else if (sample->dropvalidation == ACCESSIBILITY_CANE_DROP_CONNECTED_DESCENT) {
		sample->drop = false;
		sample->terrain = -1;
		sample->terrainheight = path->finaldelta;
		sample->terraindistance = distance;
		sample->audiosource.x = observer->origin.x + sample->direction.x * distance;
		sample->audiosource.y = observer->camera.y;
		sample->audiosource.z = observer->origin.z + sample->direction.z * distance;
		sample->evaluation.observation.drop = false;
		sample->evaluation.observation.terrain = -1;
		sample->evaluation.observation.terrainheight = path->finaldelta;
		sample->evaluation.observation.terraindistance = distance;
		sample->evaluation.cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
		sample->evaluation.source = ACCESSIBILITY_CANE_SOURCE_TERRAIN;
	}
}

static void referenceCaneSetPathPosition(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer, f32 distance)
{
	sample->audiosource.x = observer->origin.x + sample->direction.x * distance;
	sample->audiosource.y = observer->camera.y;
	sample->audiosource.z = observer->origin.z + sample->direction.z * distance;
}

static void referenceCaneSelectPathBarrier(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer,
		f32 *barrierdistance, s32 *result, s32 crouchfound)
{
	struct accessibilitycanepathdiagnostic *diag = &sample->pathdiagnostic;
	f32 distance = diag->path.stopdistance;

	/* Preserve the original sweep's exact contact when it agrees with the
	 * connected segment. Otherwise use the first blocked path step. */
	if (*result == CDRESULT_COLLISION && *barrierdistance > 0.0f
			&& *barrierdistance <= distance + sample->radius) {
		distance = *barrierdistance;
	} else {
		*result = CDRESULT_COLLISION;
		sample->result = CDRESULT_COLLISION;
		sample->obstacle = (struct prop *)diag->blocker;
		sample->obstacletype = sample->obstacle ? sample->obstacle->type : -1;
		sample->geoflags = 0;
		memset(&sample->normal, 0, sizeof(sample->normal));
		memset(&sample->edge1, 0, sizeof(sample->edge1));
		memset(&sample->edge2, 0, sizeof(sample->edge2));
		sample->rawhit.x = observer->origin.x + sample->direction.x * distance;
		sample->rawhit.y = observer->camera.y;
		sample->rawhit.z = observer->origin.z + sample->direction.z * distance;
	}

	*barrierdistance = distance;
	sample->drop = false;
	sample->terrain = 0;
	accessibilityCaneEvaluateSample(sample, distance, false, crouchfound);
}

static void referenceCaneApplyTerrainValidation(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer,
		f32 *barrierdistance, s32 *result,
		s32 legacyterrainfound, s32 crouchfound)
{
	struct accessibilitycanepathresult *path = &sample->pathdiagnostic.path;
	f32 minimumrunway = sample->radius
			* ACCESSIBILITY_CANE_PATH_TERRAIN_MIN_RUNWAY_RADII;
	f32 distance;

	if (sample->dropvalidation != ACCESSIBILITY_CANE_DROP_NOT_RUN
			|| sample->evaluation.cue == ACCESSIBILITY_CANE_CUE_LADDER
			|| (sample->evaluation.cue == ACCESSIBILITY_CANE_CUE_CROUCH
				&& path->cue != ACCESSIBILITY_CANE_CUE_TERRAIN)
			|| (sample->geoflags
				& (GEOFLAG_LADDER | GEOFLAG_LADDER_PLAYERONLY)) != 0
			|| (!legacyterrainfound
				&& path->cue != ACCESSIBILITY_CANE_CUE_TERRAIN)) {
		return;
	}

	sample->terrainvalidation = accessibilityCaneValidateTerrain(path,
			minimumrunway);
	switch (sample->terrainvalidation) {
	case ACCESSIBILITY_CANE_TERRAIN_CONFIRMED:
		distance = path->cuedistance;
		sample->drop = false;
		sample->terrain = path->direction;
		sample->terrainheight = path->cuedelta;
		sample->terraindistance = distance;
		sample->terraintraversaltested = true;
		sample->terrainclearanceresult = CDRESULT_NOCOLLISION;
		sample->terrainclearancequeries = 1;
		sample->terrainclearancedistance = path->reached;
		referenceCaneSetPathPosition(sample, observer, distance);
		accessibilityCaneEvaluateSample(sample, *barrierdistance, true,
				crouchfound);
		/* Connected clearance and runway supersede the legacy local-plane
		 * ceding heuristics for this terrain cue. */
		sample->evaluation.cue = ACCESSIBILITY_CANE_CUE_TERRAIN;
		sample->evaluation.source = ACCESSIBILITY_CANE_SOURCE_TERRAIN;
		sample->evaluation.terraincedes = false;
		sample->evaluation.shortdeadend = false;
		sample->terraintraversable = true;
		sample->terrainblockedrise = false;
		sample->terrainshortdeadend = false;
		sample->terrainsuppressed = false;
		sample->terrainrunway = path->reached - path->cuedistance;
		if (sample->terrainrunway < 0.0f) sample->terrainrunway = 0.0f;
		break;
	case ACCESSIBILITY_CANE_TERRAIN_BARRIER_FIRST:
		referenceCaneSelectPathBarrier(sample, observer,
				barrierdistance, result, crouchfound);
		break;
	case ACCESSIBILITY_CANE_TERRAIN_CONNECTED_FLAT:
		if (legacyterrainfound) {
			sample->drop = false;
			sample->terrain = 0;
			accessibilityCaneEvaluateSample(sample, *barrierdistance,
					false, crouchfound);
		}
		break;
	case ACCESSIBILITY_CANE_TERRAIN_EDGE:
		distance = path->cuedistance;
		sample->drop = true;
		sample->terrain = -2;
		sample->terrainheight = -sample->dropheightthreshold;
		sample->terraindistance = distance;
		referenceCaneSetPathPosition(sample, observer, distance);
		accessibilityCaneEvaluateSample(sample, *barrierdistance, true, false);
		break;
	default:
		break;
	}
}

static void referenceCaneApplyCrouchValidation(
		struct accessibilitycanesample *sample,
		const struct accessibilityobserver *observer,
		f32 *barrierdistance, s32 *result)
{
	struct accessibilitycanepathresult *path = &sample->pathdiagnostic.path;
	f32 minimumcontinuation = sample->radius
			* ACCESSIBILITY_CANE_PATH_CROUCH_MIN_CONTINUATION_RADII;
	f32 distance;

	/* A ladder remains the more specific traversal cue. Short, incomplete, or
	 * unsupported paths deliberately leave the established query in charge. */
	if (path->cue != ACCESSIBILITY_CANE_CUE_CROUCH
			|| sample->evaluation.cue == ACCESSIBILITY_CANE_CUE_LADDER
			|| (sample->geoflags
				& (GEOFLAG_LADDER | GEOFLAG_LADDER_PLAYERONLY)) != 0) {
		return;
	}

	sample->crouchvalidation = accessibilityCaneValidateCrouch(path,
			minimumcontinuation);

	if (sample->crouchvalidation == ACCESSIBILITY_CANE_CROUCH_DEAD_END) {
		/* A complete connected query that remains crouch-only until a wall is
		 * negative passage evidence, not an uncertain legacy fallback. */
		sample->crouchpassage = false;
		sample->crouchpass = 0;
		sample->crouchresult = CDRESULT_COLLISION;
		referenceCaneSelectPathBarrier(sample, observer, barrierdistance,
				result, false);
		return;
	}

	if (sample->crouchvalidation != ACCESSIBILITY_CANE_CROUCH_CONFIRMED) {
		return;
	}

	/* Put the cue inside the first fully proven low-clearance segment rather
	 * than on its near boundary. This makes its spatial position describe the
	 * opening while retaining two radii of verified continuation beyond it. */
	distance = path->cuedistance + sample->radius;
	if (distance > path->reached) {
		distance = path->reached;
	}
	if (distance < 0.0f) {
		distance = 0.0f;
	}

	*result = CDRESULT_COLLISION;
	*barrierdistance = distance;
	sample->result = CDRESULT_COLLISION;
	sample->drop = false;
	sample->terrain = 0;
	sample->crouchpassage = true;
	sample->crouchpass = 1;
	sample->crouchresult = CDRESULT_NOCOLLISION;
	sample->crouchymax = g_Vars.currentplayer
			? g_Vars.currentplayer->vv_manground + path->requiredheight
			: observer->origin.y + path->requiredheight;
	sample->obstacle = NULL;
	sample->obstacletype = -1;
	sample->rawhit.x = observer->origin.x + sample->direction.x * distance;
	sample->rawhit.y = observer->camera.y;
	sample->rawhit.z = observer->origin.z + sample->direction.z * distance;
	referenceCaneSetPathPosition(sample, observer, distance);
	accessibilityCaneEvaluateSample(sample, distance, false, true);
}
