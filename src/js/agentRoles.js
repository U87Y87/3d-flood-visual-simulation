// Role names follow manuscript sections 2.2.2, 2.4.2, and 3.1.
export const AGENT_ROLES = {
  requirementAnalyst: {
    name: 'Requirement Analyst',
    role: 'Requirement parsing',
    icon: 'RA',
    description: 'Interprets user requirements and constraints and generates standardized task instructions.',
  },
  taskCreator: {
    name: 'Task Creator',
    role: 'Task decomposition',
    icon: 'TC',
    description: 'Decomposes instructions into task nodes and initializes their parameters.',
  },
  decisionMaker: {
    name: 'Decision Maker',
    role: 'Service matching',
    icon: 'DM',
    description: 'Coordinates collaboration, matches atomic services, and dispatches tasks according to constraints and node states.',
  },
  taskExecutor: {
    name: 'Task Executor',
    role: 'Atomic service execution',
    icon: 'TE',
    description: 'Invokes atomic services for preprocessing, simulation, visualization, and analysis.',
  },
  organizer: {
    name: 'Organizer',
    role: 'Result validation and integration',
    icon: 'OR',
    description: 'Reports task counts, participating agents, and execution order to the Decision Maker, then validates and integrates outputs.',
  },
}
